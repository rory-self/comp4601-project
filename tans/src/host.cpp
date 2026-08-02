// On-fabric host for the SIMD + wide-AXI static tANS kernel ("tree method").
//   SOFTWARE reference : the same tANS coder on the ARM (measured 52.5 M sym/s --
//                        tANS is cheap, so this is a genuinely fast baseline).
//   HARDWARE           : arith_kernel(in64, n, out64, out_len), K=4 lanes SIMD.
// Verifies losslessness by decoding the board's output, then times both.
//
// Workload = files that SHARE one frequency table (the tree method's use case);
// the table baked into the kernel was built from that shared distribution.

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

#include "tans.h"        // normalize / build_enc / build_dec (host-side model)
#include "tans_table.h"  // TANS_L, the baked table (same one the kernel uses)

namespace {
using Clock = std::chrono::high_resolution_clock;
using byte  = std::uint8_t;

constexpr int kMaxIn   = 16384;
constexpr int kKWay    = 4;
constexpr int kChunk   = kMaxIn / kKWay;
constexpr int kChunkCap= kChunk * 2 + 64;
constexpr int kSlot    = (kChunkCap + 7) & ~7;
constexpr int kHdr     = ((4 * kKWay) + 7) & ~7;
constexpr int kOutBytes= kHdr + kKWay * kSlot + 64;   // worst case, generously sized

// The shared distribution the table was built from (must match gen_demo).
void shared_norm(int norm[256]) {
    double p[256], sum = 0;
    for (int s = 0; s < 256; ++s) { p[s] = std::pow(0.95, std::fabs(s - 128.0)) + 1e-4; sum += p[s]; }
    long cnt[256]; long tot = 0;
    for (int s = 0; s < 256; ++s) { cnt[s] = (long)(p[s] / sum * (1 << 20)) + 1; tot += cnt[s]; }
    tans::normalize(cnt, tot, TANS_L, norm);
}

// Software tANS encode of one 16 KB block, split K ways -- the SAME efficient
// byte-packing algorithm the kernel runs (NOT tans::encode, whose BitStack does a
// vector push_back per BIT and would be a ~3x too-slow, unfair baseline).
int sw_one(const byte* buf, int n, byte* cout) {
    if (n == 0) { cout[0] = 0; cout[1] = 0; return 2; }
    std::uint32_t s0 = buf[n-1], nb0 = (TANS_DNB[s0] + (1u << 15)) >> 16;
    std::uint32_t state = (nb0 << 16) - TANS_DNB[s0];
    state = TANS_STATE[(state >> nb0) + TANS_DFS[s0]];
    std::uint32_t acc = 0, tb = 0; int nacc = 0, oi = 2;
    for (int i = n - 2; i >= 0; --i) {
        std::uint32_t s = buf[i], nb = (state + TANS_DNB[s]) >> 16;
        acc = (acc << nb) | (state & ((1u << nb) - 1u)); nacc += (int)nb; tb += nb;
        if (nacc >= 8) { cout[oi++] = (byte)(acc >> (nacc - 8)); nacc -= 8; }
        if (nacc >= 8) { cout[oi++] = (byte)(acc >> (nacc - 8)); nacc -= 8; }
        state = TANS_STATE[(state >> nb) + TANS_DFS[s]];
    }
    acc = (acc << TANS_L) | (state & (TANS_SIZE - 1)); nacc += TANS_L; tb += TANS_L;
    if (nacc >= 8) { cout[oi++] = (byte)(acc >> (nacc - 8)); nacc -= 8; }
    if (nacc >= 8) { cout[oi++] = (byte)(acc >> (nacc - 8)); nacc -= 8; }
    if (nacc >  0) { cout[oi++] = (byte)(acc << (8 - nacc)); }
    cout[0] = (byte)(tb & 0xff); cout[1] = (byte)((tb >> 8) & 0xff);
    return oi;
}
int sw_encode(const tans::Enc&, const byte* in, int n, std::vector<byte>& scratch) {
    if ((int)scratch.size() < kKWay * kChunkCap) scratch.resize(kKWay * kChunkCap);
    int total = 0;
    const int chunk = (n + kKWay - 1) / kKWay;
    for (int c = 0; c < kKWay; ++c) {
        int start = c * chunk, len = std::min(chunk, n - start);
        if (len <= 0) continue;
        total += sw_one(in + start, len, scratch.data() + (std::size_t)c * kChunkCap);
    }
    return total;
}

std::string_view arg_of(std::span<char*> a, std::string_view f, std::string_view d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) return a[i + 1];
    return d;
}
int int_arg(std::span<char*> a, std::string_view f, int d) {
    const auto s = arg_of(a, f, ""); int v = d;
    std::from_chars(s.data(), s.data() + s.size(), v); return v;
}
} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string xclbin{arg_of(args, "-x", "arith.xclbin")};
    const std::string dir   {arg_of(args, "-d", ".")};
    const int iters = int_arg(args, "-n", 500);

    const std::string mode{arg_of(args, "-m", "both")};   // sw | hw | both
    const int secs = int_arg(args, "-t", 10);             // sustained-load seconds

    int norm[256]; shared_norm(norm);
    tans::Enc enc; tans::build_enc(enc, norm, TANS_L);
    tans::Dec dec; tans::build_dec(dec, norm, TANS_L);

    // Load the demo files that share the frequency table.
    std::vector<std::vector<byte>> files;
    for (int i = 0; i < 4; ++i) {
        std::ifstream f(dir + "/file" + std::to_string(i) + ".bin", std::ios::binary);
        if (!f) continue;
        std::vector<byte> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (!d.empty()) files.push_back(std::move(d));
    }
    if (files.empty()) { std::cerr << "no demo files found in " << dir << "\n"; return 2; }

    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);
    auto kernel = xrt::kernel(device, uuid, "arith_kernel");
    auto bo_in  = xrt::bo(device, kMaxIn,      kernel.group_id(0));
    auto bo_out = xrt::bo(device, kOutBytes,   kernel.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), kernel.group_id(3));
    auto* in  = bo_in.map<byte*>();
    auto* out = bo_out.map<byte*>();
    auto* len = bo_len.map<int*>();

    // Sustained-load modes: run ONE engine flat out for -t seconds so board power
    // can be sampled cleanly (energy per byte = power x time / bytes).
    if (mode == "sw" || mode == "hw") {
        std::vector<byte> scratch;
        const byte* src = files[0].data();
        std::memcpy(in, src, kMaxIn);
        bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        long long done = 0;
        const auto t0 = Clock::now();
        while (std::chrono::duration<double>(Clock::now() - t0).count() < secs) {
            for (int r = 0; r < 50; ++r) {
                if (mode == "sw") sw_encode(enc, src, kMaxIn, scratch);
                else              kernel(bo_in, kMaxIn, bo_out, bo_len).wait();
                done += kMaxIn;
            }
        }
        const double el = std::chrono::duration<double>(Clock::now() - t0).count();
        std::cout << "MODE=" << mode << " bytes=" << done << " seconds=" << el
                  << " throughput=" << (done / el / 1e6) << " MB/s\n";
        return 0;
    }

    std::cout << "===== tree method (static tANS) : ARM software vs FPGA =====\n"
              << "workload: " << files.size() << " files sharing ONE frequency table"
              << " (table precomputed once, baked into the kernel)\n"
              << "block size: " << kMaxIn << " B, K=" << kKWay << " lanes SIMD, 64-bit AXI\n\n";

    double sw_us_tot = 0, hw_us_tot = 0; long bytes_tot = 0, comp_tot = 0;
    bool all_lossless = true;
    std::vector<byte> scratch, rec(kMaxIn);

    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        const auto& F = files[fi];
        const int nblk = (int)(F.size() / kMaxIn);
        double sw_us = 0, hw_us = 0; long comp = 0; bool ok = true;

        for (int b = 0; b < nblk; ++b) {
            const byte* src = F.data() + (std::size_t)b * kMaxIn;

            // ---- software (ARM) ----
            const auto t0 = Clock::now();
            for (int r = 0; r < iters; ++r) sw_encode(enc, src, kMaxIn, scratch);
            sw_us += std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / iters;

            // ---- hardware (FPGA) ----
            std::memcpy(in, src, kMaxIn);
            bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
            kernel(bo_in, kMaxIn, bo_out, bo_len).wait();          // warm
            const auto t1 = Clock::now();
            for (int r = 0; r < iters; ++r) kernel(bo_in, kMaxIn, bo_out, bo_len).wait();
            hw_us += std::chrono::duration<double, std::micro>(Clock::now() - t1).count() / iters;
            bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            comp += len[0];

            // ---- verify the board's output decodes back to the input ----
            std::size_t rp = 0;
            for (int c = 0; c < kKWay; ++c) {
                const int rlen = out[4*c] | (out[4*c+1] << 8);
                const byte* ch = out + kHdr + (std::size_t)c * kSlot;
                long tb = ch[0] | (ch[1] << 8), cur = tb;
                auto bit = [&](long j){ return (ch[2 + (j >> 3)] >> (7 - (j & 7))) & 1; };
                auto rd  = [&](int nb){ cur -= nb; std::uint32_t v = 0;
                                        for (int k = 0; k < nb; ++k) v = (v << 1) | bit(cur + k); return v; };
                if (rlen <= 0) continue;
                std::uint32_t st = rd(TANS_L);
                for (int k = 0; k < rlen; ++k) {
                    rec[rp++] = dec.symbol[st];
                    int nb = dec.nbBits[st];
                    if (k == rlen - 1) break;
                    st = dec.newState[st] + rd(nb);
                }
            }
            if (rp != (std::size_t)kMaxIn || std::memcmp(rec.data(), src, kMaxIn) != 0) ok = false;
        }
        if (!ok) all_lossless = false;
        const long fb = (long)nblk * kMaxIn;
        std::cout << "  file" << fi << ": " << fb << " B  " << (ok ? "LOSSLESS" : "FAIL")
                  << "  ratio " << (100.0 * comp / fb) << "%"
                  << "   ARM " << (fb / sw_us) << " M sym/s"
                  << " | FPGA " << (fb / hw_us) << " M sym/s"
                  << "  -> " << (sw_us / hw_us) << "x\n";
        sw_us_tot += sw_us; hw_us_tot += hw_us; bytes_tot += fb; comp_tot += comp;
    }

    std::cout << "\n----------------------------- TOTAL -----------------------------\n"
              << "bytes             : " << bytes_tot << "\n"
              << "compressed        : " << comp_tot << " (" << (100.0 * comp_tot / bytes_tot) << "%)\n"
              << "lossless          : " << (all_lossless ? "YES (board output decoded == input)" : "NO") << "\n"
              << "ARM  software     : " << sw_us_tot << " us  (" << (bytes_tot / sw_us_tot) << " M sym/s)\n"
              << "FPGA tANS kernel  : " << hw_us_tot << " us  (" << (bytes_tot / hw_us_tot) << " M sym/s)\n"
              << "SPEEDUP           : " << (sw_us_tot / hw_us_tot) << "x\n"
              << "=================================================================\n";
    return all_lossless ? 0 : 1;
}
