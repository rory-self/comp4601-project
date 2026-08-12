/*
 * Total-payload-size benchmark for the M-coder kernel.
 *
 * This answers a different question from bench_host.cpp.  bench_host measures
 * ONE kernel call of N bytes, repeated -- it characterises the per-call cost
 * curve (t = t_fixed + N*t_sym).  This host compresses a WHOLE payload of
 * arbitrary size the way a real caller must: by splitting it into
 * ceil(total/block) successive kernel calls, exactly as demo_host does for an
 * image.  It therefore measures how the system scales with the amount of data
 * you actually have, rather than with the size of one call.
 *
 * Two knobs, both host-side, so a single bitstream covers the whole 2-D sweep:
 *   -S <bytes>   total payload
 *   -B <bytes>   per-call block, clamped to MC_MAX_IN (the kernel's compiled-in
 *                staging-array size -- a smaller block is always legal, larger
 *                is not)
 *
 * Reported at the three boundaries overhead.h defines, because they answer
 * different questions and the gap between them IS the result at small payloads:
 *   1. kernel only        sum of enqueue+wait over every call
 *   2. + DMA              what one pass over this payload actually costs
 *   3. + one-time setup   what a process that runs once and exits experiences
 *
 * Correctness is checked on an untimed pass: every block is decoded on the host
 * with ../src/mcoder_dec.cpp (the same single decoder the rest of the project
 * links) and compared against the input.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <algorithm>

#include "mcoder.h"
#include "overhead.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

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
          "usage: size_bench [-x xclbin] [-S total_bytes] [-B block_bytes]\n"
          "                  [-n passes] [-f file] [-q]\n"
          "  -S  total payload size (default 65536)\n"
          "  -B  per-call block size, clamped to MC_MAX_IN (default MC_MAX_IN)\n"
          "  -n  timed passes over the whole payload (default 5)\n"
          "  -f  use a real file's bytes, tiled/truncated to -S, instead of\n"
          "      the synthetic 'a'+(i%7) pattern\n"
          "  -q  one-line CSV output, for sweeps\n"
          "  -w  legacy whole-BO syncs: bo.sync(dir) with no size, which flushes\n"
          "      the ENTIRE buffer object however small the payload.  Default is\n"
          "      sized syncs, which move only the bytes that matter.  See\n"
          "      results/transfer_overhead_result.txt.\n";
        return 0;
    }
    const std::string xclbin = arg_of(argc, argv, "-x", "mcoder.bin");
    const std::string file   = arg_of(argc, argv, "-f", "");
    long total  = std::stol(arg_of(argc, argv, "-S", "65536"));
    int  block  = std::stoi(arg_of(argc, argv, "-B", "0"));
    int  passes = std::stoi(arg_of(argc, argv, "-n", "5"));
    const bool csv   = has_flag(argc, argv, "-q");
    const bool whole = has_flag(argc, argv, "-w");

    if (block <= 0 || block > MC_MAX_IN) block = MC_MAX_IN;
    if (total < 1) total = 1;
    if (passes < 1) passes = 1;

    ovh::Breakdown oh;

    /* ---- payload ---- */
    std::vector<mc_byte> src((size_t)total);
    std::string src_desc;
    if (!file.empty()) {
        /* Bulk read, not istreambuf_iterator: the iterator idiom goes through
         * the streambuf one byte at a time and reallocates as it grows, which
         * measures ~27 MB/s on this board and would be reported as file-I/O
         * cost when it is really the read idiom.
         *
         * The timer scope covers the file read ONLY.  Tiling the file out to
         * -S bytes is harness work, not I/O, and including it (as an RAII timer
         * spanning the whole block would) inflates the reported read cost. */
        std::vector<mc_byte> raw;
        {
            ovh::Scope rd(oh.input_read);
            std::ifstream f(file, std::ios::binary | std::ios::ate);
            if (!f) { std::cerr << "cannot open " << file << "\n"; return 2; }
            const std::streamsize sz = f.tellg();
            if (sz <= 0) { std::cerr << "empty file " << file << "\n"; return 2; }
            f.seekg(0, std::ios::beg);
            raw.resize((size_t)sz);
            f.read(reinterpret_cast<char *>(raw.data()), sz);
            if (!f) { std::cerr << "short read on " << file << "\n"; return 2; }
        }
        for (long i = 0; i < total; i++) src[i] = raw[i % raw.size()];
        src_desc = file;
    } else {
        for (long i = 0; i < total; i++) src[i] = (mc_byte)('a' + (i % 7));
        src_desc = "synthetic 'a'+(i%7)";
    }

    const long nblk = (total + block - 1) / block;

    /* ---- device setup: the one-time cost, timed by leg ---- */
    auto t = ovh::Clock::now();
    auto device = xrt::device(0);
    oh.device_open = ovh::us_since(t);

    t = ovh::Clock::now();
    auto uuid = device.load_xclbin(xclbin);
    oh.xclbin_load = ovh::us_since(t);

    t = ovh::Clock::now();
    auto krnl = xrt::kernel(device, uuid, "arith_kernel");
    oh.kernel_open = ovh::us_since(t);

    t = ovh::Clock::now();
    auto bo_in  = xrt::bo(device, MC_MAX_IN,   krnl.group_id(0));
    auto bo_out = xrt::bo(device, MC_MAX_OUT,  krnl.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), krnl.group_id(3));
    auto hin  = bo_in.map<mc_byte *>();
    auto hout = bo_out.map<mc_byte *>();
    auto hlen = bo_len.map<int *>();
    oh.bo_alloc = ovh::us_since(t);

    /*
     * Sync helpers.  The sized output path has an ordering constraint that the
     * whole-BO path hides: you cannot size the output sync until you know how
     * many bytes the kernel produced, and that length lives in its own BO.  So
     * sync the 4-byte length BO first, read clen, then sync exactly clen bytes
     * of output -- two syscalls instead of one, but moving ~5x less data at the
     * ratios this coder achieves.
     */
    auto sync_in = [&](int n) {
        if (whole) bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        else       bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE, n, 0);
    };
    auto sync_out = [&]() -> int {          /* returns clen */
        if (whole) {
            bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            return hlen[0];
        }
        bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE, sizeof(int), 0);
        const int clen = hlen[0];
        if (clen > 0 && clen <= MC_MAX_OUT)
            bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE, clen, 0);
        return clen;
    };

    /* ---- correctness pass (untimed) ----
     * Runs in whichever sync mode is selected, so a sized sync that failed to
     * make the right bytes coherent would show up here as a round-trip
     * mismatch rather than passing silently. */
    long comp_total = 0;
    bool ok = true;
    {
        std::vector<mc_byte> dec(block);
        for (long b = 0; b < nblk && ok; b++) {
            const long off = b * (long)block;
            const int  n   = (int)std::min((long)block, total - off);
            memcpy(hin, src.data() + off, n);
            sync_in(n);
            krnl(bo_in, n, bo_out, bo_len).wait();
            const int clen = sync_out();
            if (clen <= 0 || clen > MC_MAX_OUT) { ok = false; break; }
            comp_total += clen;
            const int dn = mc_decode(hout, clen, dec.data());
            if (dn != n || memcmp(dec.data(), src.data() + off, n) != 0) ok = false;
        }
    }

    /* ---- timed passes ---- */
    double krn_us = 0, h2d_us = 0, d2h_us = 0;
    for (int p = 0; p < passes; p++) {
        for (long b = 0; b < nblk; b++) {
            const long off = b * (long)block;
            const int  n   = (int)std::min((long)block, total - off);

            t = ovh::Clock::now();
            memcpy(hin, src.data() + off, n);
            sync_in(n);
            h2d_us += ovh::us_since(t);

            t = ovh::Clock::now();
            krnl(bo_in, n, bo_out, bo_len).wait();
            krn_us += ovh::us_since(t);

            t = ovh::Clock::now();
            sync_out();
            d2h_us += ovh::us_since(t);
        }
    }
    oh.hw_compute = krn_us / passes;
    oh.h2d        = h2d_us / passes;
    oh.d2h        = d2h_us / passes;
    oh.bytes      = total;

    const double mss_kernel = total / oh.hw_compute;               /* M sym/s */
    const double mss_dma    = total / oh.hw_offload();
    const double mss_e2e    = total / (oh.hw_setup() + oh.hw_offload());

    if (csv) {
        /* sync,total,block,nblk,ratio,kernel_us,h2d_us,d2h_us,dma_us,setup_us,
           mss_kernel,mss_dma,mss_e2e,lossless */
        std::cout << (whole ? "whole" : "sized") << ","
                  << total << "," << block << "," << nblk << ","
                  << (100.0 * comp_total / total) << ","
                  << oh.hw_compute << "," << oh.h2d << "," << oh.d2h << ","
                  << oh.hw_offload() << "," << oh.hw_setup() << ","
                  << mss_kernel << "," << mss_dma << "," << mss_e2e << ","
                  << (ok ? "YES" : "NO") << "\n";
        return ok ? 0 : 1;
    }

    std::cout << "M-coder total-size benchmark   MC_MAX_IN=" << MC_MAX_IN
              << "  KWAY=" << MC_KWAY << "\n"
              << "payload : " << total << " B (" << src_desc << ")\n"
              << "block   : " << block << " B  ->  " << nblk << " kernel call"
              << (nblk == 1 ? "" : "s") << " per pass\n"
              << "passes  : " << passes << "\n"
              << "sync    : " << (whole ? "whole-BO (legacy)" : "sized") << "\n"
              << "----------------------------------------------\n"
              << (ok ? "PASS: every block round-tripped losslessly\n"
                     : "FAIL: round-trip mismatch\n")
              << "compressed: " << comp_total << " B ("
              << (100.0 * comp_total / total) << "%)\n"
              << "----------------------------------------------\n"
              << "per pass over the whole payload:\n"
              << "  kernel only     : " << oh.hw_compute  << " us  (" << mss_kernel << " M sym/s)\n"
              << "  + DMA           : " << oh.hw_offload() << " us  (" << mss_dma    << " M sym/s)\n"
              << "  + one-time setup: " << (oh.hw_setup() + oh.hw_offload())
              << " us  (" << mss_e2e << " M sym/s)\n";

    ovh::report_overhead(oh);
    return ok ? 0 : 1;
}
