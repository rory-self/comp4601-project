// demo_host -- SOFTWARE vs HARDWARE: compresses an image on the ARM and on the
//               FPGA, times both, and proves the reconstruction is pixel-perfect.
// Visual demo: compress + decompress a grayscale image, CPU vs FPGA, on the board.
//   - loads a P5 (.pgm) image
//   - compresses it block-by-block (4 KB blocks) with (a) the ARM CPU coder and
//     (b) the FPGA kernel, timing each
//   - decompresses (CPU) and checks the reconstruction is pixel-perfect (lossless)
//   - prints an ASCII preview + a stats table, and writes reconstructed.pgm
// Build (C++20): aarch64 cross-compile against the XRT sysroot, with arith5.cpp.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

namespace {

using byte  = std::uint8_t;
using Clock = std::chrono::high_resolution_clock;

constexpr int kKWay   = 8;
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

// CPU decoder (to reconstruct the image from the compressed bytes).
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
            else if (low_ >= kHalf)                        { code_ -= kHalf;     low_ -= kHalf;     high_ -= kHalf; }
            else if (low_ >= kFirstQtr && high_ < kThirdQtr) { code_ -= kFirstQtr; low_ -= kFirstQtr; high_ -= kFirstQtr; }
            else break;
            low_ = (low_ << 1) & kTop; high_ = ((high_ << 1) | 1) & kTop; code_ = ((code_ << 1) | next_bit()) & kTop;
        }
        if (bit == 0) prob += (kProbTotal - prob) >> kMoveBits; else prob -= prob >> kMoveBits;
        return bit;
    }
};

std::vector<byte> decode_kway(std::span<const byte> comp) {
    std::vector<byte> out; std::size_t off = 2 * kKWay;
    for (int k = 0; k < kKWay; ++k) {
        const int len = comp[2 * k] | (comp[2 * k + 1] << 8);
        const auto chunk = RangeDecoder{comp.subspan(off, len)}.decode_chunk();
        out.insert(out.end(), chunk.begin(), chunk.end());
        off += len;
    }
    return out;
}

struct Image { int w = 0, h = 0; std::vector<byte> px; };

Image read_pgm(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::string magic; int maxv; Image img;
    f >> magic >> img.w >> img.h >> maxv; f.get();
    if (magic != "P5") throw std::runtime_error("need a binary P5 PGM");
    img.px.resize(static_cast<std::size_t>(img.w) * img.h);
    f.read(reinterpret_cast<char*>(img.px.data()), img.px.size());
    return img;
}
void write_pgm(const std::string& path, const Image& img) {
    std::ofstream f(path, std::ios::binary);
    f << "P5\n" << img.w << ' ' << img.h << "\n255\n";
    f.write(reinterpret_cast<const char*>(img.px.data()), img.px.size());
}

void ascii_preview(const Image& img, std::string_view title) {
    constexpr std::string_view ramp = " .:-=+*#%@";
    const int cols = 64, rows = std::max(1, cols * img.h / img.w / 2);
    std::cout << title << ":\n";
    for (int r = 0; r < rows; ++r) {
        std::string line;
        for (int c = 0; c < cols; ++c) {
            const int x = c * img.w / cols, y = r * img.h / rows;
            line += ramp[img.px[static_cast<std::size_t>(y) * img.w + x] * 9 / 255];
        }
        std::cout << "  " << line << '\n';
    }
}

std::string arg_of(std::span<char*> a, std::string_view f, std::string_view d) {
    for (std::size_t i = 1; i + 1 < a.size(); ++i) if (f == a[i]) return a[i + 1];
    return std::string{d};
}

} // namespace

int arith_encode(const byte*, int, byte*);   // CPU coder (arith5.cpp)

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    const std::string img_path = arg_of(args, "-i", "image.pgm");
    const std::string xclbin   = arg_of(args, "-x", "arith.bin");

    const Image img = read_pgm(img_path);
    const int total = img.w * img.h;
    const int nblk  = (total + kMaxIn - 1) / kMaxIn;

    std::cout << "================ IMAGE COMPRESSION DEMO ================\n"
              << "image: " << img_path << "  " << img.w << 'x' << img.h << " = " << total
              << " bytes, " << nblk << " blocks of <=" << kMaxIn << "\n\n";
    ascii_preview(img, "ORIGINAL");

    // ---- FPGA setup ----
    auto device = xrt::device(0);
    const auto uuid = device.load_xclbin(xclbin);
    auto kernel = xrt::kernel(device, uuid, "arith_kernel");
    auto bo_in  = xrt::bo(device, kMaxIn,      kernel.group_id(0));
    auto bo_out = xrt::bo(device, kMaxOut,     kernel.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), kernel.group_id(3));
    auto* in  = bo_in.map<byte*>();
    auto* out = bo_out.map<byte*>();
    auto* len = bo_len.map<int*>();

    std::vector<std::vector<byte>> comp_cpu(nblk), comp_fpga(nblk);
    std::array<byte, kMaxOut> obuf{};

    // ---- CPU compress (timed) ----
    const auto tc0 = Clock::now();
    for (int b = 0; b < nblk; ++b) {
        const int off = b * kMaxIn, n = std::min(kMaxIn, total - off);
        const int cl = arith_encode(img.px.data() + off, n, obuf.data());
        comp_cpu[b].assign(obuf.begin(), obuf.begin() + cl);
    }
    const double cpu_us = std::chrono::duration<double, std::micro>(Clock::now() - tc0).count();
    long comp_bytes = 0; for (const auto& v : comp_cpu) comp_bytes += static_cast<long>(v.size());

    // ---- FPGA compress (timed) ----
    const auto tf0 = Clock::now();
    for (int b = 0; b < nblk; ++b) {
        const int off = b * kMaxIn, n = std::min(kMaxIn, total - off);
        std::memcpy(in, img.px.data() + off, n);
        bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        kernel(bo_in, n, bo_out, bo_len).wait();
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        comp_fpga[b].assign(out, out + len[0]);
    }
    const double fpga_us = std::chrono::duration<double, std::micro>(Clock::now() - tf0).count();

    // ---- reconstruct from the FPGA-compressed data (CPU decode) ----
    Image recon{img.w, img.h, std::vector<byte>(total)};
    std::size_t rp = 0;
    for (const auto& block : comp_fpga) {
        const auto d = decode_kway(block);
        std::copy(d.begin(), d.end(), recon.px.begin() + rp);
        rp += d.size();
    }
    const bool lossless = rp == static_cast<std::size_t>(total) && recon.px == img.px;
    bool same = true;
    for (int b = 0; b < nblk && same; ++b) same = (comp_cpu[b] == comp_fpga[b]);

    write_pgm("reconstructed.pgm", recon);
    std::cout << '\n';
    ascii_preview(recon, "RECONSTRUCTED (from FPGA-compressed bytes)");

    std::cout << "\n===================== RESULTS =====================\n"
              << "original size      : " << total << " bytes\n"
              << "compressed size    : " << comp_bytes << " bytes  ("
              << (100.0 * comp_bytes / total) << "% of original)\n"
              << "lossless           : " << (lossless ? "YES  (reconstructed == original, pixel-perfect)" : "NO") << '\n'
              << "CPU vs FPGA output : " << (same ? "identical bytes" : "differ") << '\n'
              << "---------------------------------------------------\n"
              << "ARM CPU  compress  : " << cpu_us  << " us  (" << (total / cpu_us)  << " MB/s)\n"
              << "FPGA     compress  : " << fpga_us << " us  (" << (total / fpga_us) << " MB/s)\n"
              << "SPEEDUP (FPGA/CPU) : " << (cpu_us / fpga_us) << "x\n"
              << "===================================================\n"
              << "wrote reconstructed.pgm\n";
    return lossless ? 0 : 1;
}
