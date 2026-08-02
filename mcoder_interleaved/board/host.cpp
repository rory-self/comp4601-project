// On-fabric host for the interleaved M-coder (KV260, XRT).
//   - SOFTWARE reference: single-stream M-coder on the ARM (same CABAC coder,
//     the honest CPU baseline), timed.
//   - HARDWARE: the interleaved M-coder kernel on the FPGA, timed.
//   - verifies both are lossless (M-coder decode), prints throughput + speedup.
// Build (C++20): aarch64 cross-compile against the XRT sysroot, with mcoder.h.

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

#include "mcoder.h"   // Bryan's M-coder primitives (mc_enc_*, mc_dec_*)

namespace {

using Clock = std::chrono::high_resolution_clock;
constexpr int kLanes  = 16;         // == LANES in the kernel
constexpr int kMaxIn  = MC_MAX_IN;
constexpr int kMaxOut = MC_MAX_OUT;

// Single-stream M-coder encode (== mc_encode_chunk with K=1). Software reference.
int mc_sw_encode(const mc_byte* in, int n, mc_byte* out) {
    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
    mc_enc e; mc_enc_init(&e, out);
    for (int k = 0; k < n; k++) {
        int b = in[k], ctx = 1;
        for (int j = 7; j >= 0; j--) { int bit = (b >> j) & 1; mc_encode_bin(&e, &tree[ctx], bit); ctx = (ctx << 1) | bit; }
    }
    mc_enc_flush(&e);
    return e.pk.oi;
}

// Decode one M-coder chunk of n bytes.
int mc_dec_chunk(const mc_byte* in, int len, int n, mc_byte* out) {
    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
    mc_dec d; mc_dec_init(&d, in, len);
    for (int k = 0; k < n; k++) {
        int b = 0, ctx = 1;
        for (int j = 7; j >= 0; j--) { int bit = mc_decode_bin(&d, &tree[ctx]); b = (b << 1) | bit; ctx = (ctx << 1) | bit; }
        out[k] = (mc_byte)b;
    }
    return n;
}

// Decode the K-lane container: [K x {u16 rlen, u16 clen}] then chunks.
int decode_container(const mc_byte* comp, int comp_len, int k, mc_byte* out) {
    if (comp_len < 4 * k) return -1;
    int off = 4 * k, on = 0;
    for (int c = 0; c < k; c++) {
        int rlen = comp[4 * c]     | (comp[4 * c + 1] << 8);
        int clen = comp[4 * c + 2] | (comp[4 * c + 3] << 8);
        if (off + clen > comp_len) return -1;
        on  += mc_dec_chunk(comp + off, clen, rlen, out + on);
        off += clen;
    }
    return on;
}

std::string_view arg_of(std::span<char*> a, std::string_view f, std::string_view d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) return a[i + 1];
    return d;
}
int int_arg(std::span<char*> a, std::string_view f, int d) {
    const auto s = arg_of(a, f, ""); int v = d; std::from_chars(s.data(), s.data() + s.size(), v); return v;
}

} // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string xclbin{arg_of(args, "-x", "arith.xclbin")};
    const int iters = int_arg(args, "-n", 2000);
    const int n     = std::min(int_arg(args, "-N", 4095), kMaxIn);

    std::vector<mc_byte> input(n);
    for (int i = 0; i < n; ++i) input[i] = static_cast<mc_byte>('a' + (i % 7));

    // ---------- SOFTWARE reference (single-stream M-coder on ARM) ----------
    std::array<mc_byte, kMaxOut> sw_out{};
    std::vector<mc_byte> sw_dec(kMaxIn);
    const int sw_len = mc_sw_encode(input.data(), n, sw_out.data());
    const int sw_dn  = mc_dec_chunk(sw_out.data(), sw_len, n, sw_dec.data());  // bare chunk (no header)
    const bool sw_ok = sw_dn == n && std::equal(input.begin(), input.end(), sw_dec.begin());
    std::vector<double> sw_us; sw_us.reserve(iters);
    for (int k = 0; k < iters; ++k) {
        const auto t0 = Clock::now();
        mc_sw_encode(input.data(), n, sw_out.data());
        sw_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
    }

    // ---------- HARDWARE (interleaved M-coder kernel) ----------
    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);
    auto kernel = xrt::kernel(device, uuid, "arith_kernel");
    auto bo_in  = xrt::bo(device, kMaxIn,      kernel.group_id(0));
    auto bo_out = xrt::bo(device, kMaxOut,     kernel.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), kernel.group_id(3));
    auto* in  = bo_in.map<mc_byte*>();
    auto* out = bo_out.map<mc_byte*>();
    auto* len = bo_len.map<int*>();
    std::copy(input.begin(), input.end(), in);
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    kernel(bo_in, n, bo_out, bo_len).wait();
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    const int hw_len = len[0];
    std::vector<mc_byte> hw_dec(kMaxIn);
    const int hw_dn = decode_container(out, hw_len, kLanes, hw_dec.data());
    const bool hw_ok = hw_dn == n && std::equal(input.begin(), input.end(), hw_dec.begin());
    std::vector<double> hw_us; hw_us.reserve(iters);
    for (int k = 0; k < iters; ++k) {
        const auto t0 = Clock::now();
        kernel(bo_in, n, bo_out, bo_len).wait();
        hw_us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
    }

    auto mean = [](std::vector<double>& v){ std::ranges::sort(v); return std::reduce(v.begin(), v.end()) / v.size(); };
    const double sw = mean(sw_us), hw = mean(hw_us);

    std::cout << "========= SW reference (ARM) vs HW (FPGA interleaved M-coder) =========\n"
              << "input                  : " << n << " symbols (compressible pattern)\n"
              << "SW compressed          : " << sw_len << " B (" << (100.0 * sw_len / n) << "%)  single-stream\n"
              << "HW compressed          : " << hw_len << " B (" << (100.0 * hw_len / n) << "%)  " << kLanes << " chunks\n"
              << "SW lossless            : " << (sw_ok ? "YES" : "NO") << "\n"
              << "HW lossless            : " << (hw_ok ? "YES" : "NO") << "\n"
              << "----------------------------------------------------------------------\n"
              << "ARM  software M-coder  : " << sw << " us/call   (" << (n / sw) << " M sym/s)\n"
              << "FPGA interleaved kernel: " << hw << " us/call   (" << (n / hw) << " M sym/s)\n"
              << "SPEEDUP (HW vs SW)     : " << (sw / hw) << "x\n"
              << "======================================================================\n";
    return (sw_ok && hw_ok) ? 0 : 1;
}
