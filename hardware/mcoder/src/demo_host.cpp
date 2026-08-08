/*
 * Visual demo for the V7 M-coder: compress + decompress a grayscale image,
 * ARM CPU vs FPGA, on the board.
 *
 *   - loads a P5 (.pgm, grayscale) or P6 (.ppm, colour) image
 *   - compresses it in 4 KB blocks with (a) the ARM CPU coder and (b) the FPGA
 *     kernel, timing each
 *   - decodes the FPGA's output on the CPU and checks the reconstruction is
 *     pixel-perfect
 *   - prints ASCII previews + a stats table, writes reconstructed.pgm
 *   - reports the speedup at three boundaries (see ../../common/overhead.h):
 *     kernel only, kernel + DMA, and end-to-end including the image read, the
 *     device open, the xclbin load and buffer allocation -- plus the break-even
 *     payload size, which is the honest answer to "was offloading worth it?"
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
 * The coder itself is format-agnostic -- it compresses bytes and has no idea
 * they are pixels.  But feed it a PNG or JPEG and it will *expand* them
 * (measured: 101.5%), because those are already entropy-coded and look like
 * noise to an order-0 byte model.  Use uncompressed formats.
 *
 * -o writes the compressed stream as a real file (.mcz) so the size is visible
 * to `ls`, and -d reads one back and reconstructs the image -- so the demo can
 * show compression and decompression as separate, checkable steps.
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
#include "overhead.h"

#ifndef MC_NO_XRT
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#endif

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

/*
 * V5's published board numbers (board/onfabric_result.txt), used only for the
 * indicative comparison lines below.
 *
 * NOTE these are NOT like-for-like with this demo.  V5's 13.29 was measured on
 * 4095 bytes of synthetic 'a'+(i%7), one call, timed around enqueue+wait only.
 * This demo runs a real image over 16 sequential calls and times memcpy + both
 * DMA syncs as well.  Different workload, different measurement boundary.
 *
 * The rigorous V5 comparison is host/mc_host.cpp, which reproduces V5's
 * benchmark exactly -- same 'a'+(i%7) pattern, same N=4095, same 2000
 * iterations, same enqueue+wait boundary -- and measures 2.34x.  Quote that one.
 */
#define V5_MSPS   13.29
#define ARM_MSPS   3.46

/*
 * .mcz container -- self-describing so a compressed file can stand alone:
 *   "MCZ1" | u16 W | u16 H | u8 channels | u8 reserved | u32 nblk
 *   then nblk x { u32 len, len bytes }
 */
static void put_u16(std::ofstream &f, unsigned v) { f.put((char)(v & 0xFF)); f.put((char)(v >> 8)); }
static void put_u32(std::ofstream &f, unsigned v) {
    for (int i = 0; i < 4; i++) f.put((char)((v >> (8 * i)) & 0xFF));
}
static unsigned get_u16(std::ifstream &f) { int a = f.get(), b = f.get(); return (unsigned)(a | (b << 8)); }
static unsigned get_u32(std::ifstream &f) {
    unsigned v = 0; for (int i = 0; i < 4; i++) v |= (unsigned)f.get() << (8 * i); return v;
}

static void ascii_preview(const std::vector<mc_byte> &px, int W, int H, int ch,
                          const char *title) {
    const char *ramp = " .:-=+*#%@";
    int cols = 64, rows = cols * H / W / 2;
    if (rows < 1) rows = 1;
    std::cout << title << ":\n";
    for (int r = 0; r < rows; r++) {
        std::string line;
        for (int c = 0; c < cols; c++) {
            int x = c * W / cols, y = r * H / rows;
            size_t i = ((size_t)y * W + x) * ch;
            int v = (ch == 3) ? (px[i] + px[i + 1] + px[i + 2]) / 3 : px[i];
            line += ramp[v * 9 / 255];
        }
        std::cout << "  " << line << "\n";
    }
}

int main(int argc, char **argv) {
    std::string img = "image.pgm", xclbin = "mcoder.bin", mcz = "", dec = "";
    double mhz = 200.0;                   /* KV260 platform clock */
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "-i")) img    = argv[i + 1];
        if (!strcmp(argv[i], "-x")) xclbin = argv[i + 1];
        if (!strcmp(argv[i], "-c")) mhz    = std::stod(argv[i + 1]);
        if (!strcmp(argv[i], "-o")) mcz    = argv[i + 1];   /* write compressed */
        if (!strcmp(argv[i], "-d")) dec    = argv[i + 1];   /* decompress only  */
    }

    /* ---- standalone decompress: .mcz -> image, no board needed ---- */
    if (!dec.empty()) {
        std::ifstream g(dec, std::ios::binary);
        if (!g) { std::cerr << "cannot open " << dec << "\n"; return 1; }
        char m[4]; g.read(m, 4);
        if (memcmp(m, "MCZ1", 4)) { std::cerr << dec << " is not an .mcz file\n"; return 1; }
        int Wd = (int)get_u16(g), Hd = (int)get_u16(g);
        int chd = g.get(); g.get();
        unsigned nb = get_u32(g);
        std::vector<mc_byte> out((size_t)Wd * Hd * chd);
        static mc_byte cbuf[MC_MAX_OUT], obuf2[MC_MAX_IN];
        size_t op = 0;
        for (unsigned b = 0; b < nb; b++) {
            unsigned len = get_u32(g);
            if (len > MC_MAX_OUT) { std::cerr << "block " << b << ": bad length\n"; return 1; }
            g.read((char *)cbuf, len);
            int dn = mc_decode(cbuf, (int)len, obuf2);
            if (dn < 0 || op + (size_t)dn > out.size()) {
                std::cerr << "block " << b << ": decode rejected the stream\n"; return 1;
            }
            memcpy(out.data() + op, obuf2, dn); op += (size_t)dn;
        }
        std::string name = (chd == 3) ? "decompressed.ppm" : "decompressed.pgm";
        std::ofstream o(name, std::ios::binary);
        o << (chd == 3 ? "P6\n" : "P5\n") << Wd << " " << Hd << "\n255\n";
        o.write((char *)out.data(), (long)out.size());
        o.close();
        std::cout << "decompressed " << dec << " -> " << name << "  ("
                  << Wd << "x" << Hd << ", " << chd << " channel"
                  << (chd == 3 ? "s" : "") << ", " << out.size() << " bytes)\n";
        ascii_preview(out, Wd, Hd, chd, "DECOMPRESSED");
        return 0;
    }

    /* ---- read P5 (grayscale) or P6 (colour) ---- */
    ovh::Breakdown oh;
    auto tio = ovh::Clock::now();
    std::ifstream f(img, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << img << "\n"; return 1; }
    std::string magic; int W, H, maxv;
    f >> magic >> W >> H >> maxv; f.get();
    int ch = (magic == "P6") ? 3 : 1;
    if (magic != "P5" && magic != "P6") {
        std::cerr << "need a binary P5 (.pgm) or P6 (.ppm).  Note: PNG/JPEG are\n"
                     "already entropy-coded and this coder will EXPAND them.\n";
        return 1;
    }
    std::vector<mc_byte> px((size_t)W * H * ch);
    f.read((char *)px.data(), (long)px.size());
    f.close();
    oh.input_read = ovh::us_since(tio);        /* both paths pay this */
    int total = W * H * ch, nblk = (total + MC_MAX_IN - 1) / MC_MAX_IN;
    oh.bytes = total;

    std::cout << "========== M-CODER (V7) IMAGE COMPRESSION DEMO ==========\n";
    std::cout << "image: " << img << "  " << W << "x" << H << " = " << total
              << " bytes, " << nblk << " blocks of <=" << MC_MAX_IN
              << ", K=" << MC_KWAY << ", " << ch << " channel"
              << (ch == 3 ? "s (RGB)" : "") << "\n\n";
    ascii_preview(px, W, H, ch, "ORIGINAL");

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
    oh.sw_compute = cpu_us;

    /* ---- FPGA compress (timed) ---- */
    double fpga_us = 0;
    bool have_fpga = false;
#ifndef MC_NO_XRT
    /* Setup, timed leg by leg.  None of it shows up in the kernel figure, and
     * all of it is work the CPU path never has to do. */
    auto t = ovh::Clock::now();
    auto device = xrt::device(0);
    oh.device_open = ovh::us_since(t);

    t = ovh::Clock::now();
    auto uuid   = device.load_xclbin(xclbin);
    oh.xclbin_load = ovh::us_since(t);

    t = ovh::Clock::now();
    auto krnl   = xrt::kernel(device, uuid, "arith_kernel");
    oh.kernel_open = ovh::us_since(t);

    t = ovh::Clock::now();
    auto bo_in  = xrt::bo(device, MC_MAX_IN,   krnl.group_id(0));
    auto bo_out = xrt::bo(device, MC_MAX_OUT,  krnl.group_id(2));
    auto bo_len = xrt::bo(device, sizeof(int), krnl.group_id(3));
    auto hin = bo_in.map<mc_byte *>(); auto hout = bo_out.map<mc_byte *>();
    auto hlen = bo_len.map<int *>();
    oh.bo_alloc = ovh::us_since(t);

    /* Same loop as before, split into three accumulators so the DMA can be
     * separated from the coding; the total is unchanged. */
    auto tf0 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < nblk; b++) {
        int off = b * MC_MAX_IN, n = std::min(MC_MAX_IN, total - off);
        t = ovh::Clock::now();
        memcpy(hin, px.data() + off, n);
        bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        oh.h2d += ovh::us_since(t);

        t = ovh::Clock::now();
        auto r = krnl(bo_in, n, bo_out, bo_len); r.wait();
        oh.hw_compute += ovh::us_since(t);

        t = ovh::Clock::now();
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        comp_fpga[b].assign(hout, hout + hlen[0]);
        oh.d2h += ovh::us_since(t);
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

    std::string rname = (ch == 3) ? "reconstructed.ppm" : "reconstructed.pgm";
    std::ofstream of(rname, std::ios::binary);
    of << (ch == 3 ? "P6\n" : "P5\n") << W << " " << H << "\n255\n";
    of.write((char *)recon.data(), total);
    of.close();

    /* Write the compressed stream as a real file, so its size is visible to
     * `ls` and it can be handed back to -d.  Without this the compressed data
     * only ever exists in memory and the demo has to be taken on trust. */
    if (!mcz.empty()) {
        std::ofstream c(mcz, std::ios::binary);
        c.write("MCZ1", 4);
        put_u16(c, (unsigned)W); put_u16(c, (unsigned)H);
        c.put((char)ch); c.put(0);
        put_u32(c, (unsigned)nblk);
        for (int b = 0; b < nblk; b++) {
            put_u32(c, (unsigned)comp_fpga[b].size());
            c.write((char *)comp_fpga[b].data(), (long)comp_fpga[b].size());
        }
        c.close();
    }

    std::cout << "\n";
    ascii_preview(recon, W, H, ch, "RECONSTRUCTED (decoded from the compressed bytes)");

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
        std::cout << "SPEEDUP (FPGA/CPU) : " << (cpu_us / fpga_us)
                  << "x   [kernel + DMA, setup excluded]\n";
        std::cout << "-------------------------------------------------------\n";
        std::cout << "implied cyc/byte   : " << (mhz / msps) << "  at " << mhz << " MHz\n";
        std::cout << "vs V5 on fabric    : " << (msps / V5_MSPS)
                  << "x   INDICATIVE ONLY - different workload and timing\n"
                     "                     boundary than V5's 13.29.  Use mc_host\n"
                     "                     for the like-for-like figure (2.34x).\n";
        std::cout << "vs ARM baseline    : " << (msps / ARM_MSPS)
                  << "x   (V5's published ARM figure, different workload)\n";
        std::cout << "SPEEDUP above      : FPGA vs ARM running THIS coder on THIS\n"
                     "                     image -- the sound acceleration number.\n";
    }
    std::cout << "=======================================================\n";
    std::cout << "wrote " << rname;
    if (!mcz.empty()) std::cout << " and " << mcz << "  (decompress with:  -d " << mcz << ")";
    std::cout << "\n";

    if (have_fpga) {
        ovh::report_overhead(oh);
        ovh::report_speedup(oh);
    }
    return lossless ? 0 : 1;
}
