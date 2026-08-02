/*
 * Visual demo for the V7 M-coder: compress + decompress a grayscale image,
 * ARM CPU vs FPGA, on the board.
 *
 *   - loads a P5 (.pgm) image
 *   - compresses it in 4 KB blocks with (a) the ARM CPU coder and (b) the FPGA
 *     kernel, timing each
 *   - decodes the FPGA's output on the CPU and checks the reconstruction is
 *     pixel-perfect
 *   - prints ASCII previews + a stats table, writes reconstructed.pgm
 *
 * Same structure as demo/demo_host.cpp (the V5 demo) so the two are directly
 * comparable, with three differences:
 *
 *   1. It links ../mcoder_enc.cpp and ../mcoder_dec.cpp rather than carrying a
 *      copy of the model constants and decoder.  The V5 demo inlines its own,
 *      which is how a decoder silently drifts from its encoder.
 *   2. It cross-checks CPU output against FPGA output byte-for-byte.  That is a
 *      stronger test than "it decoded": it proves the hardware implements the
 *      identical algorithm, not merely a self-consistent one.
 *   3. `make sim` builds it without XRT so the demo can be rehearsed, and the
 *      compression numbers checked, with no board attached.
 *
 * Build for the board:  make SYSROOT=<xrt-sysroot>
 * Rehearse on a laptop: make sim && ./mc_demo_sim -i ../../demo/image.pgm
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

#ifndef MC_NO_XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#endif

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

/* V5's measured board result, for the comparison line (board/onfabric_result.txt). */
#define V5_MSPS   13.29
#define ARM_MSPS   3.46

static void ascii_preview(const std::vector<mc_byte> &px, int W, int H, const char *title) {
    const char *ramp = " .:-=+*#%@";
    int cols = 64, rows = cols * H / W / 2;
    if (rows < 1) rows = 1;
    std::cout << title << ":\n";
    for (int r = 0; r < rows; r++) {
        std::string line;
        for (int c = 0; c < cols; c++) {
            int x = c * W / cols, y = r * H / rows;
            line += ramp[px[y * W + x] * 9 / 255];
        }
        std::cout << "  " << line << "\n";
    }
}

int main(int argc, char **argv) {
    std::string img = "image.pgm", xclbin = "mcoder.bin";
    double mhz = 200.0;                   /* KV260 platform clock */
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "-i")) img    = argv[i + 1];
        if (!strcmp(argv[i], "-x")) xclbin = argv[i + 1];
        if (!strcmp(argv[i], "-c")) mhz    = std::stod(argv[i + 1]);
    }

    /* ---- read P5 PGM ---- */
    std::ifstream f(img, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << img << "\n"; return 1; }
    std::string magic; int W, H, maxv;
    f >> magic >> W >> H >> maxv; f.get();
    if (magic != "P5") { std::cerr << "need a binary P5 PGM\n"; return 1; }
    std::vector<mc_byte> px((size_t)W * H);
    f.read((char *)px.data(), (long)W * H);
    f.close();
    int total = W * H, nblk = (total + MC_MAX_IN - 1) / MC_MAX_IN;

    std::cout << "========== M-CODER (V7) IMAGE COMPRESSION DEMO ==========\n";
    std::cout << "image: " << img << "  " << W << "x" << H << " = " << total
              << " bytes, " << nblk << " blocks of <=" << MC_MAX_IN
              << ", K=" << MC_KWAY << "\n\n";
    ascii_preview(px, W, H, "ORIGINAL");

    std::vector<std::vector<mc_byte>> comp_cpu(nblk), comp_fpga(nblk);
    static mc_byte obuf[MC_MAX_OUT];

    /* ---- CPU compress (timed) ---- */
    auto tc0 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < nblk; b++) {
        int off = b * MC_MAX_IN, n = std::min(MC_MAX_IN, total - off);
        int cl = mc_encode(px.data() + off, n, obuf);
        comp_cpu[b].assign(obuf, obuf + cl);
    }
    auto tc1 = std::chrono::high_resolution_clock::now();
    double cpu_us = std::chrono::duration<double, std::micro>(tc1 - tc0).count();

    /* ---- FPGA compress (timed) ---- */
    double fpga_us = 0;
    bool have_fpga = false;
#ifndef MC_NO_XRT
    auto device = xrt::device(0);
    auto uuid   = device.load_xclbin(xclbin);
    auto krnl   = xrt::kernel(device, uuid, "arith_kernel");
    auto bo_in  = xrt::bo(device, MC_MAX_IN,   krnl.group_id(0));
    auto bo_out = xrt::bo(device, MC_MAX_OUT,  krnl.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), krnl.group_id(3));
    auto hin = bo_in.map<mc_byte *>(); auto hout = bo_out.map<mc_byte *>();
    auto hlen = bo_len.map<int *>();

    auto tf0 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < nblk; b++) {
        int off = b * MC_MAX_IN, n = std::min(MC_MAX_IN, total - off);
        memcpy(hin, px.data() + off, n);
        bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        auto r = krnl(bo_in, n, bo_out, bo_len); r.wait();
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        comp_fpga[b].assign(hout, hout + hlen[0]);
    }
    auto tf1 = std::chrono::high_resolution_clock::now();
    fpga_us = std::chrono::duration<double, std::micro>(tf1 - tf0).count();
    have_fpga = true;
#else
    std::cout << "\n*** SIM BUILD: no board.  Compression figures are real;\n"
                 "    the FPGA timing comparison is skipped. ***\n";
    comp_fpga = comp_cpu;              /* reconstruct from the CPU stream */
#endif

    /* ---- reconstruct from the FPGA-compressed bytes ---- */
    std::vector<mc_byte> recon(total);
    static mc_byte dblk[MC_MAX_IN];
    int rp = 0;
    for (int b = 0; b < nblk; b++) {
        int dn = mc_decode(comp_fpga[b].data(), (int)comp_fpga[b].size(), dblk);
        if (dn < 0) { std::cerr << "block " << b << ": decode rejected the stream\n"; return 1; }
        memcpy(recon.data() + rp, dblk, dn);
        rp += dn;
    }
    bool lossless = (rp == total) && (memcmp(recon.data(), px.data(), total) == 0);

    /* Bit-exactness: does the hardware implement the same algorithm as the
     * software model, or merely a self-consistent one? */
    bool same = true;
    for (int b = 0; b < nblk && same; b++) same = (comp_cpu[b] == comp_fpga[b]);

    long comp_bytes = 0; for (auto &v : comp_fpga) comp_bytes += (long)v.size();

    std::ofstream of("reconstructed.pgm", std::ios::binary);
    of << "P5\n" << W << " " << H << "\n255\n";
    of.write((char *)recon.data(), total);
    of.close();

    std::cout << "\n";
    ascii_preview(recon, W, H, "RECONSTRUCTED (decoded from the compressed bytes)");

    std::cout << "\n======================= RESULTS =======================\n";
    std::cout << "original size      : " << total << " bytes\n";
    std::cout << "compressed size    : " << comp_bytes << " bytes  ("
              << (100.0 * comp_bytes / total) << "% of original, "
              << ((double)total / comp_bytes) << "x)\n";
    std::cout << "lossless           : "
              << (lossless ? "YES  (reconstructed == original, pixel-perfect)" : "NO") << "\n";
    if (have_fpga)
        std::cout << "CPU vs FPGA output : "
                  << (same ? "IDENTICAL bytes  (hardware matches the software model)"
                           : "DIFFER") << "\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << "ARM CPU  compress  : " << cpu_us << " us  ("
              << (total / cpu_us) << " M symbols/s)\n";
    if (have_fpga) {
        double msps = total / fpga_us;
        std::cout << "FPGA     compress  : " << fpga_us << " us  ("
                  << msps << " M symbols/s)\n";
        std::cout << "SPEEDUP (FPGA/CPU) : " << (cpu_us / fpga_us) << "x\n";
        std::cout << "-------------------------------------------------------\n";
        std::cout << "implied cyc/byte   : " << (mhz / msps) << "  at " << mhz << " MHz\n";
        std::cout << "vs V5 on fabric    : " << (msps / V5_MSPS)
                  << "x   (V5 measured " << V5_MSPS << " M sym/s)\n";
        std::cout << "vs ARM baseline    : " << (msps / ARM_MSPS)
                  << "x   (ARM measured " << ARM_MSPS << " M sym/s)\n";
    }
    std::cout << "=======================================================\n";
    std::cout << "wrote reconstructed.pgm\n";
    return lossless ? 0 : 1;
}
