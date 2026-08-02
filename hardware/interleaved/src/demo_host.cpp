// demo_host -- SOFTWARE vs HARDWARE: times the ARM coder against the FPGA kernel
//               on the same input and verifies both are lossless.
// Live SW-vs-HW demo for the time-interleaved coder (KV260, XRT).
//   - SOFTWARE reference: the efficient single-stream CPU coder (arith5, K=1) on
//     the ARM  -- the same 3.46 M sym/s baseline used throughout the project.
//   - HARDWARE: the time-interleaved kernel on the FPGA.
//   - times both around only the encode, verifies each is lossless, prints the
//     throughput + speedup table.
// Build (C++20): aarch64 cross-compile against the XRT sysroot, linking
// arith5.cpp (-DKWAY=1) as the software reference.

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "arith3.h"   // coder params shared by arith5 and the interleaved kernel

namespace {

using byte  = std::uint8_t;
using Clock = std::chrono::high_resolution_clock;

constexpr int kHwLanes = 16;     // interleaved kernel splits into 16 chunks
constexpr int kMaxIn   = MAX_IN;
constexpr int kMaxOut  = MAX_OUT;

constexpr std::uint32_t kCodeBits  = CODE_BITS;
constexpr std::uint32_t kTop       = (1u << kCodeBits) - 1;
constexpr std::uint32_t kFirstQtr  = (kTop + 1) / 4;
constexpr std::uint32_t kHalf      = 2 * kFirstQtr;
constexpr std::uint32_t kThirdQtr  = 3 * kFirstQtr;
constexpr std::uint32_t kProbBits  = PROB_BITS;
constexpr std::uint32_t kProbTotal = PROB_TOTAL;
constexpr std::uint32_t kProbInit  = PROB_INIT;
constexpr std::uint32_t kMoveBits  = MOVE_BITS;
constexpr int kNTree = NTREE;

// Host-side decoder — proves a compressed stream is lossless. Both arith5 and the
// interleaved kernel use the identical per-chunk format (continuation flag +
// 8-level bit-tree), so one decoder serves both; only the chunk count differs.
class RangeDecoder {
public:
    explicit RangeDecoder(std::span<const byte> bits) : bits_{bits} {
        for (std::uint32_t i = 0; i < kCodeBits; ++i) code_ = (code_ << 1) | next_bit();
    }
    std::vector<byte> decode_chunk() {
        std::array<std::uint32_t, kNTree> tree; tree.fill(kProbInit);
        std::uint32_t flag = kProbInit;
        std::vector<byte> out;
        while (decode_bit(flag)) {
            int ctx = 1, sym = 0;
            for (int i = 7; i >= 0; --i) { int b = decode_bit(tree[ctx]); sym = (sym << 1) | b; ctx = (ctx << 1) | b; }
            out.push_back(static_cast<byte>(sym));
        }
        return out;
    }
private:
    std::span<const byte> bits_;
    long pos_ = 0; std::uint32_t low_ = 0, high_ = kTop, code_ = 0;
    std::uint32_t next_bit() {
        const auto bi = static_cast<std::size_t>(pos_ >> 3); const int off = 7 - static_cast<int>(pos_ & 7); ++pos_;
        return bi < bits_.size() ? (bits_[bi] >> off) & 1u : 0u;
    }
    int decode_bit(std::uint32_t& prob) {
        const std::uint32_t range = high_ - low_ + 1, split = (range * prob) >> kProbBits;
        int bit;
        if ((code_ - low_) < split) { bit = 0; high_ = low_ + split - 1; } else { bit = 1; low_ = low_ + split; }
        for (;;) {
            if (high_ < kHalf) {}
            else if (low_ >= kHalf)                          { code_ -= kHalf;     low_ -= kHalf;     high_ -= kHalf; }
            else if (low_ >= kFirstQtr && high_ < kThirdQtr) { code_ -= kFirstQtr; low_ -= kFirstQtr; high_ -= kFirstQtr; }
            else break;
            low_ = (low_ << 1) & kTop; high_ = ((high_ << 1) | 1) & kTop; code_ = ((code_ << 1) | next_bit()) & kTop;
        }
        if (bit == 0) prob += (kProbTotal - prob) >> kMoveBits; else prob -= prob >> kMoveBits;
        return bit;
    }
};

// Decode a K-chunk stream (header = K x uint16 lengths, then the chunks).
std::vector<byte> decode_kway(std::span<const byte> comp, int k) {
    std::vector<byte> out; std::size_t off = 2 * k;
    for (int c = 0; c < k; ++c) {
        const int len = comp[2 * c] | (comp[2 * c + 1] << 8);
        const auto chunk = RangeDecoder{comp.subspan(off, len)}.decode_chunk();
        out.insert(out.end(), chunk.begin(), chunk.end());
        off += len;
    }
    return out;
}

std::string_view arg_of(std::span<char*> a, std::string_view f, std::string_view d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) return a[i + 1];
    return d;
}
int int_arg(std::span<char*> a, std::string_view f, int d) {
    const auto s = arg_of(a, f, ""); int v = d; std::from_chars(s.data(), s.data() + s.size(), v); return v;
}

} // namespace

int arith_encode(const byte* in, int n, byte* out);   // software reference: arith5.cpp, -DKWAY=1

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string xclbin{arg_of(args, "-x", "arith.xclbin")};
    const int iters = int_arg(args, "-n", 2000);
    const int n     = std::min(int_arg(args, "-N", 4095), kMaxIn);

    std::vector<byte> input(n);
    for (int i = 0; i < n; ++i) input[i] = static_cast<byte>('a' + (i % 7));

    // ---------- SOFTWARE reference (ARM): efficient single-stream coder ----------
    std::array<byte, kMaxOut> sw_out{};
    const int sw_len = arith_encode(input.data(), n, sw_out.data());
    const auto sw_dec = decode_kway({sw_out.data(), static_cast<std::size_t>(sw_len)}, 1);
    const bool sw_ok = static_cast<int>(sw_dec.size()) == n &&
                       std::equal(sw_dec.begin(), sw_dec.end(), input.begin());
    std::vector<double> sw_us; sw_us.reserve(iters);
    for (int k = 0; k < iters; ++k) {
        const auto t0 = Clock::now();
        arith_encode(input.data(), n, sw_out.data());
        sw_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
    }

    // ---------- HARDWARE (FPGA): interleaved kernel ----------
    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);
    auto kernel = xrt::kernel(device, uuid, "arith_kernel");
    auto bo_in  = xrt::bo(device, kMaxIn,      kernel.group_id(0));
    auto bo_out = xrt::bo(device, kMaxOut,     kernel.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), kernel.group_id(3));
    auto* in  = bo_in.map<byte*>();
    auto* out = bo_out.map<byte*>();
    auto* len = bo_len.map<int*>();
    std::copy(input.begin(), input.end(), in);
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    kernel(bo_in, n, bo_out, bo_len).wait();
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    const int hw_len = len[0];
    const auto hw_dec = decode_kway({out, static_cast<std::size_t>(hw_len)}, kHwLanes);
    const bool hw_ok = static_cast<int>(hw_dec.size()) == n &&
                       std::equal(hw_dec.begin(), hw_dec.end(), input.begin());
    std::vector<double> hw_us; hw_us.reserve(iters);
    for (int k = 0; k < iters; ++k) {
        const auto t0 = Clock::now();
        kernel(bo_in, n, bo_out, bo_len).wait();
        hw_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
    }

    auto mean = [](std::vector<double>& v){ std::ranges::sort(v); return std::reduce(v.begin(), v.end()) / v.size(); };
    const double sw = mean(sw_us), hw = mean(hw_us);

    std::cout << "============ SW reference (ARM) vs HW (FPGA interleaved) ============\n"
              << "input                  : " << n << " symbols (compressible pattern)\n"
              << "SW compressed          : " << sw_len << " B (" << (100.0 * sw_len / n) << "%)  single-stream\n"
              << "HW compressed          : " << hw_len << " B (" << (100.0 * hw_len / n) << "%)  16 chunks\n"
              << "SW lossless            : " << (sw_ok ? "YES" : "NO") << "\n"
              << "HW lossless            : " << (hw_ok ? "YES" : "NO") << "\n"
              << "--------------------------------------------------------------------\n"
              << "ARM  software (arith5) : " << sw << " us/call   (" << (n / sw) << " M sym/s)\n"
              << "FPGA interleaved kernel: " << hw << " us/call   (" << (n / hw) << " M sym/s)\n"
              << "SPEEDUP (HW vs SW)     : " << (sw / hw) << "x\n"
              << "====================================================================\n";
    return (sw_ok && hw_ok) ? 0 : 1;
}
