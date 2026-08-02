// multi_host -- COMPUTE-UNIT SCALING: runs 1 CU then 2 CUs from the same bitstream
//                to isolate the effect of instancing.
// Multi-instance experiment: the same bitstream contains TWO compute units
// (arith_kernel_1, arith_kernel_2) on separate HP port pairs. Running 1 CU vs 2
// CUs from the same binary isolates the effect of instancing alone.
//
// Question it answers: we sat at ~16% LUT / 34% BRAM with one kernel. Does
// spending the idle chip on a second copy actually double throughput, or is
// something shared (DDR bandwidth, XRT dispatch) the real limit?

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "tans.h"
#include "tans_table.h"

namespace {
using Clock = std::chrono::high_resolution_clock;
using byte  = std::uint8_t;
constexpr int kMaxIn    = 16384;
constexpr int kKWay     = 4;
constexpr int kChunk    = kMaxIn / kKWay;
constexpr int kChunkCap = kChunk * 2 + 64;
constexpr int kSlot     = (kChunkCap + 7) & ~7;
constexpr int kHdr      = ((4 * kKWay) + 7) & ~7;
constexpr int kOutBytes = kHdr + kKWay * kSlot + 64;

void shared_norm(int norm[256]) {
    double p[256], sum = 0;
    for (int s = 0; s < 256; ++s) { p[s] = std::pow(0.95, std::fabs(s - 128.0)) + 1e-4; sum += p[s]; }
    long cnt[256]; long tot = 0;
    for (int s = 0; s < 256; ++s) { cnt[s] = (long)(p[s] / sum * (1 << 20)) + 1; tot += cnt[s]; }
    tans::normalize(cnt, tot, TANS_L, norm);
}
int int_arg(std::span<char*> a, std::string_view f, int d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) { int v = d;
        std::string s{a[i+1]}; std::from_chars(s.data(), s.data() + s.size(), v); return v; }
    return d;
}
std::string str_arg(std::span<char*> a, std::string_view f, std::string d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) return a[i+1];
    return d;
}

// One CU's worth of state: its own kernel handle and its own buffers.
struct Unit {
    xrt::kernel k; xrt::bo in, out, len;
    byte* pin; byte* pout; int* plen;
};
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string xclbin = str_arg(args, "-x", "arith_multi.xclbin");
    const std::string dir    = str_arg(args, "-d", ".");
    const int iters = int_arg(args, "-n", 300);

    int norm[256]; shared_norm(norm);
    tans::Dec dec; tans::build_dec(dec, norm, TANS_L);

    std::ifstream f(dir + "/file0.bin", std::ios::binary);
    std::vector<byte> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if ((int)data.size() < kMaxIn) { std::cerr << "need file0.bin >= 16 KB\n"; return 2; }

    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);

    // Open each compute unit by name -- this is what makes them run concurrently.
    std::vector<Unit> u;
    for (const char* cu : {"arith_kernel:{arith_kernel_1}", "arith_kernel:{arith_kernel_2}"}) {
        xrt::kernel k(device, uuid, cu);
        xrt::bo bi(device, kMaxIn, k.group_id(0));
        xrt::bo bo(device, kOutBytes, k.group_id(2));
        xrt::bo bl(device, sizeof(int), k.group_id(3));
        Unit x{k, bi, bo, bl, bi.map<byte*>(), bo.map<byte*>(), bl.map<int*>()};
        std::memcpy(x.pin, data.data(), kMaxIn);
        x.in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        u.push_back(std::move(x));
    }
    std::cout << "opened " << u.size() << " compute units from " << xclbin << "\n\n";

    auto verify = [&](Unit& x) {
        x.out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); x.len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        std::vector<byte> rec; rec.reserve(kMaxIn);
        for (int c = 0; c < kKWay; ++c) {
            const int rlen = x.pout[4*c] | (x.pout[4*c+1] << 8);
            const byte* ch = x.pout + kHdr + (std::size_t)c * kSlot;
            long tb = ch[0] | (ch[1] << 8), cur = tb;
            auto bit = [&](long j){ return (ch[2 + (j>>3)] >> (7 - (j&7))) & 1; };
            auto rd  = [&](int nb){ cur -= nb; std::uint32_t v = 0;
                                    for (int k2 = 0; k2 < nb; ++k2) v = (v<<1) | bit(cur+k2); return v; };
            if (rlen <= 0) continue;
            std::uint32_t st = rd(TANS_L);
            for (int k2 = 0; k2 < rlen; ++k2) { rec.push_back(dec.symbol[st]);
                int nb = dec.nbBits[st]; if (k2 == rlen-1) break; st = dec.newState[st] + rd(nb); }
        }
        return rec.size() == (std::size_t)kMaxIn && std::memcmp(rec.data(), data.data(), kMaxIn) == 0;
    };

    // ---- 1 CU ----
    u[0].k(u[0].in, kMaxIn, u[0].out, u[0].len).wait();
    const bool ok1 = verify(u[0]);
    auto t0 = Clock::now();
    for (int r = 0; r < iters; ++r) u[0].k(u[0].in, kMaxIn, u[0].out, u[0].len).wait();
    double us1 = std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / iters;

    // ---- 2 CUs, both in flight, wait for both ----
    auto r1 = u[0].k(u[0].in, kMaxIn, u[0].out, u[0].len);
    auto r2 = u[1].k(u[1].in, kMaxIn, u[1].out, u[1].len);
    r1.wait(); r2.wait();
    const bool ok2 = verify(u[0]) && verify(u[1]);
    t0 = Clock::now();
    for (int r = 0; r < iters; ++r) {
        auto a = u[0].k(u[0].in, kMaxIn, u[0].out, u[0].len);
        auto b = u[1].k(u[1].in, kMaxIn, u[1].out, u[1].len);
        a.wait(); b.wait();
    }
    double us2 = std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / iters;

    const double tp1 = kMaxIn / us1, tp2 = 2.0 * kMaxIn / us2;
    std::cout << "=============== multi-instance (same bitstream) ===============\n"
              << "1 CU  : " << us1 << " us/call    " << tp1 << " M sym/s   lossless=" << (ok1?"YES":"NO") << "\n"
              << "2 CUs : " << us2 << " us/pair    " << tp2 << " M sym/s   lossless=" << (ok2?"YES":"NO") << "\n"
              << "--------------------------------------------------------------\n"
              << "scaling: " << (tp2/tp1) << "x   (2.00x = perfect)\n"
              << "==============================================================\n";
    return (ok1 && ok2) ? 0 : 1;
}
