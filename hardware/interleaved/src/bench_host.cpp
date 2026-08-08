// bench_host -- HARDWARE ONLY: profiles the kernel (chrono around enqueue+wait)
//                and verifies the board's output is lossless. No software comparison.
// On-fabric host for the time-interleaved arithmetic-coding kernel (KV260, XRT).
//   - loads the xclbin and runs arith_kernel(in, n, out, out_len) on the PL
//   - verifies losslessness (decode the board's output, compare to the input)
//   - profiles with std::chrono around ONLY the kernel enqueue + wait
//   - AND accounts for what that boundary excludes: device open, xclbin load,
//     buffer allocation, and the per-call DMA in both directions.  No software
//     baseline here (demo_host does that), so the overhead is reported as an
//     amortisation curve -- how much data one process must push before the
//     fixed setup stops dominating.
// Build (C++20): aarch64 cross-compile against the XRT sysroot, -I../../common.
//
// NOTE vs the K-way host: the interleaved kernel splits the message into
// LANES = 16 chunks, so the compressed stream header is 16 x uint16 lengths and
// there are 16 chunks to decode.  The per-chunk stream format (continuation
// flag + 8-level bit-tree, WNC renorm) is identical, so the decoder is unchanged
// apart from kKWay = 16.

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "overhead.h"

namespace {

using byte  = std::uint8_t;
using Clock = std::chrono::high_resolution_clock;

constexpr int kKWay   = 16;      // == LANES in the interleaved kernel
constexpr int kMaxIn  = 4096;
constexpr int kMaxOut = 8192;

constexpr std::uint32_t kCodeBits  = 16;
constexpr std::uint32_t kTop       = (1u << kCodeBits) - 1;
constexpr std::uint32_t kFirstQtr  = (kTop + 1) / 4;
constexpr std::uint32_t kHalf      = 2 * kFirstQtr;
constexpr std::uint32_t kThirdQtr  = 3 * kFirstQtr;
constexpr std::uint32_t kProbBits  = 12;
constexpr std::uint32_t kProbTotal = 1u << kProbBits;
constexpr std::uint32_t kProbInit  = kProbTotal / 2;
constexpr std::uint32_t kMoveBits  = 5;
constexpr int kNTree = 256;

class RangeDecoder {
public:
    explicit RangeDecoder(std::span<const byte> bits) : bits_{bits} { reset(); }
    std::vector<byte> decode_chunk() {
        std::array<std::uint32_t, kNTree> tree;
        tree.fill(kProbInit);
        std::uint32_t flag = kProbInit;
        std::vector<byte> out;
        while (decode_bit(flag)) {
            int ctx = 1, sym = 0;
            for (int i = 7; i >= 0; --i) {
                const int b = decode_bit(tree[ctx]);
                sym = (sym << 1) | b;
                ctx = (ctx << 1) | b;
            }
            out.push_back(static_cast<byte>(sym));
        }
        return out;
    }
private:
    std::span<const byte> bits_;
    long          pos_  = 0;
    std::uint32_t low_  = 0, high_ = kTop, code_ = 0;
    void reset() { for (std::uint32_t i = 0; i < kCodeBits; ++i) code_ = (code_ << 1) | next_bit(); }
    std::uint32_t next_bit() {
        const auto byte_idx = static_cast<std::size_t>(pos_ >> 3);
        const int  off = 7 - static_cast<int>(pos_ & 7);
        ++pos_;
        return byte_idx < bits_.size() ? (bits_[byte_idx] >> off) & 1u : 0u;
    }
    int decode_bit(std::uint32_t& prob) {
        const std::uint32_t range = high_ - low_ + 1;
        const std::uint32_t split = (range * prob) >> kProbBits;
        int bit;
        if ((code_ - low_) < split) { bit = 0; high_ = low_ + split - 1; }
        else                        { bit = 1; low_ = low_ + split; }
        for (;;) {
            if (high_ < kHalf) {
            } else if (low_ >= kHalf)                        { code_ -= kHalf;     low_ -= kHalf;     high_ -= kHalf; }
            else if (low_ >= kFirstQtr && high_ < kThirdQtr) { code_ -= kFirstQtr; low_ -= kFirstQtr; high_ -= kFirstQtr; }
            else break;
            low_  = (low_ << 1) & kTop;
            high_ = ((high_ << 1) | 1) & kTop;
            code_ = ((code_ << 1) | next_bit()) & kTop;
        }
        if (bit == 0) prob += (kProbTotal - prob) >> kMoveBits;
        else          prob -= prob >> kMoveBits;
        return bit;
    }
};

std::vector<byte> decode_kway(std::span<const byte> comp) {
    std::vector<byte> out;
    std::size_t offset = 2 * kKWay;
    for (int k = 0; k < kKWay; ++k) {
        const int len = comp[2 * k] | (comp[2 * k + 1] << 8);
        const auto chunk = RangeDecoder{comp.subspan(offset, len)}.decode_chunk();
        out.insert(out.end(), chunk.begin(), chunk.end());
        offset += len;
    }
    return out;
}

std::string_view arg_of(std::span<char*> args, std::string_view flag, std::string_view fallback) {
    for (std::size_t i = 1; i + 1 < args.size(); ++i) if (flag == args[i]) return args[i + 1];
    return fallback;
}
int int_arg(std::span<char*> args, std::string_view flag, int fallback) {
    const auto s = arg_of(args, flag, "");
    int v = fallback;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string xclbin{arg_of(args, "-x", "arith.xclbin")};
    const int dev   = int_arg(args, "-d", 0);
    const int iters = int_arg(args, "-n", 2000);
    const int n     = std::min(int_arg(args, "-N", 4095), kMaxIn);

    // Payload generated host-side and copied in below, so the copy is charged to
    // transfer rather than hidden inside payload generation.
    std::vector<byte> src(n);
    for (int i = 0; i < n; ++i) src[i] = static_cast<byte>('a' + (i % 7));

    ovh::Breakdown oh;
    oh.bytes = n;

    std::cout << "Open device " << dev << "\nLoad xclbin " << xclbin << '\n';
    auto t = ovh::Clock::now();
    auto device = xrt::device(dev);
    oh.device_open = ovh::us_since(t);

    t = ovh::Clock::now();
    const auto uuid = device.load_xclbin(xclbin);
    oh.xclbin_load = ovh::us_since(t);

    t = ovh::Clock::now();
    auto kernel = xrt::kernel(device, uuid, "arith_kernel");
    oh.kernel_open = ovh::us_since(t);

    // Buffer args at kernel indices 0 (in), 2 (out), 3 (out_len); index 1 is scalar n.
    t = ovh::Clock::now();
    auto bo_in  = xrt::bo(device, kMaxIn,      kernel.group_id(0));
    auto bo_out = xrt::bo(device, kMaxOut,     kernel.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), kernel.group_id(3));
    auto* in  = bo_in.map<byte*>();
    auto* out = bo_out.map<byte*>();
    auto* len = bo_len.map<int*>();
    oh.bo_alloc = ovh::us_since(t);

    std::memcpy(in, src.data(), n);
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    kernel(bo_in, n, bo_out, bo_len).wait();
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    const int  clen = len[0];
    const auto decoded = decode_kway({out, static_cast<std::size_t>(clen)});
    const bool lossless = static_cast<int>(decoded.size()) == n &&
                          std::equal(decoded.begin(), decoded.end(), in);

    std::cout << "----------------------------------------------\n"
              << "input " << n << " symbols -> compressed " << clen
              << " bytes (" << (100.0 * clen / n) << "%)\n"
              << (lossless ? "PASS: board output is lossless (decode == input)\n"
                           : "FAIL: round-trip mismatch\n")
              << "----------------------------------------------\n";

    std::vector<double> samples;
    samples.reserve(iters);
    for (int k = 0; k < iters; ++k) {
        const auto t0 = Clock::now();
        kernel(bo_in, n, bo_out, bo_len).wait();
        const auto t1 = Clock::now();
        samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    std::ranges::sort(samples);
    const double mean   = std::reduce(samples.begin(), samples.end()) / samples.size();
    const double median = samples[samples.size() / 2];

    std::cout << "\n=== On-fabric kernel profiling (chrono around krnl()+wait only) ===\n"
              << "iterations       : " << iters << '\n'
              << "mean per call    : " << mean << " us  (" << n << " symbols)\n"
              << "median per call  : " << median << " us\n"
              << "min/max per call : " << samples.front() << " / " << samples.back() << " us\n"
              << "per-symbol       : " << (mean * 1000.0 / n) << " ns/sym\n"
              << "throughput       : " << (n / mean) << " M symbols/s\n";

    // --- the same call with its data movement, timed by leg ---
    // A real caller cannot skip these: the payload has to reach the device and
    // the compressed bytes have to come back.  Averaged over `iters`.
    double h2d = 0, comp = 0, d2h = 0;
    for (int k = 0; k < iters; ++k) {
        t = ovh::Clock::now();
        std::memcpy(in, src.data(), n);
        bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        h2d += ovh::us_since(t);

        t = ovh::Clock::now();
        kernel(bo_in, n, bo_out, bo_len).wait();
        comp += ovh::us_since(t);

        t = ovh::Clock::now();
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        d2h += ovh::us_since(t);
    }
    oh.h2d = h2d / iters;
    oh.hw_compute = comp / iters;
    oh.d2h = d2h / iters;

    std::cout << "\n=== one call WITH its data movement ===\n"
              << "kernel only      : " << oh.hw_compute << " us  (" << (n / oh.hw_compute) << " M symbols/s)\n"
              << "kernel + DMA     : " << oh.hw_offload() << " us  (" << (n / oh.hw_offload()) << " M symbols/s)\n"
              << "cost of offload  : " << (100.0 * oh.hw_move() / oh.hw_offload()) << "% of the call is data movement\n";

    ovh::report_overhead(oh);
    ovh::report_amortised(oh);
    std::cout << "(no software baseline here -- demo_host does the SW-vs-HW comparison\n"
                 " with the same three boundaries.)\n";
    return lossless ? 0 : 1;
}
