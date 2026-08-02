// Generic multi-compute-unit scaling test. Works for ANY of our kernels, since
// they all expose the same signature:  arith_kernel(in, n, out, out_len).
//
//   ./multi_cu -x <xclbin> -c <numCUs> [-N bytes] [-n iters]
//
// Correctness without needing each design's decoder: every CU is given the SAME
// input, so every CU must produce byte-identical output, and that output must
// match what a single CU produces (which each design's own host already verified
// to be lossless). Any divergence means the CUs are interfering.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using Clock = std::chrono::high_resolution_clock;
using byte  = std::uint8_t;

static std::string sarg(int c, char** v, const std::string& f, const std::string& d) {
    for (int i = 1; i + 1 < c; ++i) if (f == v[i]) return v[i + 1];
    return d;
}
static int iarg(int c, char** v, const std::string& f, int d) {
    const std::string s = sarg(c, v, f, "");
    if (s.empty()) return d;
    int x = d; std::from_chars(s.data(), s.data() + s.size(), x); return x;
}

int main(int argc, char** argv) {
    const std::string xclbin = sarg(argc, argv, "-x", "arith.xclbin");
    const int ncu    = iarg(argc, argv, "-c", 2);
    const int n      = iarg(argc, argv, "-N", 4095);
    const int iters  = iarg(argc, argv, "-n", 500);
    const int maxout = iarg(argc, argv, "-O", 65536);

    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);

    struct Unit { xrt::kernel k; xrt::bo in, out, len; byte* pin; byte* pout; int* plen; };
    std::vector<Unit> u;
    for (int i = 1; i <= ncu; ++i) {
        const std::string cu = "arith_kernel:{arith_kernel_" + std::to_string(i) + "}";
        try {
            xrt::kernel k(device, uuid, cu);
            xrt::bo bi(device, std::max(n, 4096), k.group_id(0));
            xrt::bo bo(device, maxout, k.group_id(2));
            xrt::bo bl(device, sizeof(int), k.group_id(3));
            Unit x{k, bi, bo, bl, bi.map<byte*>(), bo.map<byte*>(), bl.map<int*>()};
            for (int j = 0; j < n; ++j) x.pin[j] = (byte)('a' + (j % 7));   // same input for all
            x.in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            u.push_back(std::move(x));
        } catch (const std::exception& e) {
            std::cerr << "could not open " << cu << ": " << e.what() << "\n";
            return 2;
        }
    }
    std::cout << "opened " << u.size() << " compute unit(s), n=" << n << " bytes\n";

    // ---- reference: 1 CU ----
    u[0].k(u[0].in, n, u[0].out, u[0].len).wait();
    u[0].out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); u[0].len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    const int ref_len = u[0].plen[0];
    std::vector<byte> ref(u[0].pout, u[0].pout + ref_len);

    auto t0 = Clock::now();
    for (int r = 0; r < iters; ++r) u[0].k(u[0].in, n, u[0].out, u[0].len).wait();
    const double us1 = std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / iters;

    // ---- all CUs concurrently ----
    std::vector<xrt::run> runs;
    runs.reserve(u.size());
    for (auto& x : u) runs.push_back(x.k(x.in, n, x.out, x.len));
    for (auto& r : runs) r.wait();
    runs.clear();

    bool same = true;
    for (auto& x : u) {
        x.out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); x.len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        if (x.plen[0] != ref_len || std::memcmp(x.pout, ref.data(), ref_len) != 0) same = false;
    }

    t0 = Clock::now();
    for (int r = 0; r < iters; ++r) {
        for (auto& x : u) runs.push_back(x.k(x.in, n, x.out, x.len));
        for (auto& rr : runs) rr.wait();
        runs.clear();
    }
    const double usN = std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / iters;

    const double tp1 = n / us1, tpN = (double)n * u.size() / usN;
    std::cout << "compressed        : " << ref_len << " bytes (" << (100.0*ref_len/n) << "%)\n"
              << "all CUs identical : " << (same ? "YES" : "NO -- CUs interfere!") << "\n"
              << "-----------------------------------------------------------\n"
              << "1 CU    : " << us1 << " us   " << tp1 << " M sym/s\n"
              << u.size() << " CUs   : " << usN << " us   " << tpN << " M sym/s\n"
              << "scaling : " << (tpN/tp1) << "x   (" << u.size() << ".00x = perfect)\n";
    return same ? 0 : 1;
}
