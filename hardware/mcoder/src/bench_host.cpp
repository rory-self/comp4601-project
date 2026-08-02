/*
 * On-fabric XRT host for the V7 M-coder kernel (KV260).
 *
 *   - loads the xclbin and runs arith_kernel(in, n, out, out_len) on the PL
 *   - verifies losslessness by decoding the board's output on the host
 *   - profiles with std::chrono around ONLY the kernel enqueue + wait
 *
 * Two deliberate differences from board/host.cpp (V5's host):
 *
 * 1. It does NOT carry its own copy of the decoder.  V5's host inlined one and
 *    documented that its constants "must match the kernel / arith3.h" -- a
 *    standing invitation for the two to drift, and drift is silent: a
 *    mismatched decoder yields garbage and reports a round-trip failure without
 *    saying why.  This links ../mcoder_dec.cpp, the same decoder Phase 1 and
 *    cosim validate against, so there is exactly one decoder in the project.
 *
 * 2. It can run real files (-f), not just a synthetic pattern.  V5's host
 *    measured 'a'+(i%7), which is far more compressible than real data and
 *    makes the ratio look better than it is.  Throughput is unaffected by
 *    content (8 bins/byte regardless), but the ratio very much is.
 *
 * Build for the board (aarch64, XRT sysroot):   make
 * Build a host-logic test with no XRT/board:    make sim && ./mc_host_sim
 *
 * The sim build swaps the kernel call for the software encoder so that the
 * argument handling, file IO, verification and reporting can be exercised
 * without hardware.  It does not test XRT.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <algorithm>

/* Resolved via an include path (-I), not a relative path, so this file can
 * be dropped into a Vitis IDE component directory unchanged.  See
 * host/Makefile and mcoder_host/UserConfig.cmake. */
#include "mcoder.h"

#ifndef MC_NO_XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#endif

int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);
#ifdef MC_NO_XRT
int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
#endif

static std::string arg_of(int c, char **v, const char *f, const char *d) {
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], f)) return v[i + 1];
    return d;
}
static bool has_flag(int c, char **v, const char *f) {
    for (int i = 1; i < c; i++) if (!strcmp(v[i], f)) return true;
    return false;
}

int main(int argc, char **argv) {
    if (has_flag(argc, argv, "-h")) {
        std::cout <<
          "usage: mc_host [-x xclbin] [-d dev] [-N symbols] [-n iters]\n"
          "               [-f file] [-c clock_mhz]\n"
          "  -f  compress the first N bytes of a real file instead of a\n"
          "      synthetic pattern (ratio on synthetic data is optimistic)\n"
          "  -c  kernel clock in MHz, used only to derive cycles/byte\n"
          "      (default 200 -- the KV260 platform clock the kernel runs at)\n";
        return 0;
    }
    std::string xclbin = arg_of(argc, argv, "-x", "mcoder.bin");
    /* Also accept the xclbin as a bare first argument. */
    if (argc > 1 && argv[1][0] != '-') xclbin = argv[1];
    std::string file   = arg_of(argc, argv, "-f", "");
    int dev    = std::stoi(arg_of(argc, argv, "-d", "0"));
    int N      = std::stoi(arg_of(argc, argv, "-N", "4095"));
    int iters  = std::stoi(arg_of(argc, argv, "-n", "2000"));
    /* The KV260 platform exposes only fixed clocks -- 100 / 200 / 400 MHz --
     * so the kernel runs at 200 MHz regardless of the 6.0 ns HLS constraint
     * (that constraint only steers HLS scheduling, not the hardware clock).
     * Post-route WNS at 5.000 ns is +0.754 ns, so 200 MHz is real. */
    double mhz = std::stod(arg_of(argc, argv, "-c", "200"));
    if (N > MC_MAX_IN) N = MC_MAX_IN;
#ifdef MC_NO_XRT
    (void)dev; (void)mhz; (void)xclbin;   /* only the XRT path uses these */
#endif

    std::vector<mc_byte> src(MC_MAX_IN, 0);
    std::string src_desc;
    if (!file.empty()) {
        std::ifstream f(file, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << file << "\n"; return 2; }
        f.read(reinterpret_cast<char *>(src.data()), N);
        std::streamsize got = f.gcount();
        if (got <= 0) { std::cerr << "empty file " << file << "\n"; return 2; }
        N = (int)got;
        src_desc = file + " (" + std::to_string(N) + " bytes)";
    } else {
        for (int i = 0; i < N; i++) src[i] = (mc_byte)('a' + (i % 7));
        src_desc = "synthetic 'a'+(i%7) -- optimistic ratio, use -f for real data";
    }

    std::cout << "V7 M-coder host   KWAY=" << MC_KWAY << "\ninput: " << src_desc << "\n";

#ifdef MC_NO_XRT
    std::cout << "*** SIM BUILD: software encoder, no board, no XRT ***\n";
    std::vector<mc_byte> outbuf(MC_MAX_OUT);
    mc_byte *hin = src.data(), *hout = outbuf.data();
    int clen = 0;
    auto run_once = [&]() { clen = mc_encode(hin, N, hout); };
#else
    std::cout << "open device " << dev << ", load " << xclbin << "\n";
    auto device = xrt::device(dev);
    auto uuid   = device.load_xclbin(xclbin);
    auto krnl   = xrt::kernel(device, uuid, "arith_kernel");

    auto bo_in  = xrt::bo(device, MC_MAX_IN,   krnl.group_id(0));
    auto bo_out = xrt::bo(device, MC_MAX_OUT,  krnl.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), krnl.group_id(3));
    auto hin  = bo_in.map<mc_byte *>();
    auto hout = bo_out.map<mc_byte *>();
    auto hlen = bo_len.map<int *>();

    memcpy(hin, src.data(), N);
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    int clen = 0;
    auto run_once = [&]() {
        auto r = krnl(bo_in, N, bo_out, bo_len);
        r.wait();
    };
    auto fetch = [&]() {
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        clen = hlen[0];
    };
#endif

    /* ---- correctness ---- */
    run_once();
#ifndef MC_NO_XRT
    fetch();
#endif
    if (clen <= 0 || clen > MC_MAX_OUT) {
        std::cout << "FAIL: implausible compressed length " << clen << "\n";
        return 1;
    }
    std::vector<mc_byte> dec(MC_MAX_IN);
    int dn = mc_decode(hout, clen, dec.data());
    bool ok = (dn == N) && (memcmp(dec.data(), hin, N) == 0);

    std::cout << "----------------------------------------------\n";
    std::cout << "input " << N << " symbols -> compressed " << clen
              << " bytes (" << (100.0 * clen / N) << "%)\n";
    if (!ok) {
        std::cout << "FAIL: round-trip mismatch (decoded " << dn << " of " << N << ")\n";
        std::cout << "      If the kernel is V5's, this is expected: the container\n"
                     "      layout and probability model both differ.  Check that the\n"
                     "      xclbin is the M-coder build.\n";
    } else {
        std::cout << "PASS: board output is lossless (decode == input)\n";
    }
    std::cout << "----------------------------------------------\n";

    /* ---- profiling: time only enqueue + wait ---- */
    std::vector<double> us;
    us.reserve(iters);
    for (int k = 0; k < iters; k++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        run_once();
        auto t1 = std::chrono::high_resolution_clock::now();
        us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    std::sort(us.begin(), us.end());
    double tot = 0; for (double v : us) tot += v;
    double mean = tot / us.size(), med = us[us.size() / 2];
    double msps = N / mean;                       /* symbols/us == M symbols/s */

    std::cout << "\n=== kernel profiling (chrono around enqueue+wait only) ===\n";
    std::cout << "iterations       : " << iters << "\n";
    std::cout << "mean per call    : " << mean << " us  (" << N << " symbols)\n";
    std::cout << "median per call  : " << med  << " us\n";
    std::cout << "min/max per call : " << us.front() << " / " << us.back() << " us\n";
    std::cout << "per-symbol       : " << (mean * 1000.0 / N) << " ns/sym\n";
    std::cout << "throughput       : " << msps << " M symbols/s\n";
#ifndef MC_NO_XRT
    std::cout << "implied cyc/byte : " << (mhz / msps) << "  at " << mhz << " MHz\n";
    std::cout << "                   (cosim predicted 6.67 at K=8, of which the\n"
                 "                    coder is only 1.0 -- the rest is data movement)\n";
    std::cout << "vs V5 on fabric  : " << (msps / 13.29) << "x  (V5 measured 13.29 M sym/s)\n";
#endif
    return ok ? 0 : 1;
}
