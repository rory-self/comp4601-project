/*
 * Host<->kernel data-transfer benchmark.  NO kernel is ever launched.
 *
 * Motivation: the existing hosts time "h2d" as memcpy-into-BO + sync together,
 * and call bo.sync(dir) with no size, which syncs the WHOLE buffer object
 * regardless of how many bytes the payload actually uses.  On the 64 KB kernel
 * that is 64 KB in + 128 KB out per call even for a 4 KB payload.  Both choices
 * inflate the apparent transfer cost and make it look payload-independent.
 * This isolates the pieces.
 *
 * Platform note, which frames the whole measurement: the KV260 is a Zynq
 * UltraScale+ MPSoC.  PS and PL share the same DDR -- there is no PCIe and no
 * discrete device memory.  xrt::bo::sync() is therefore cache maintenance
 * (flush before the PL reads, invalidate after it writes), not a bus copy.  The
 * numbers below are the cost of making shared memory coherent, not of moving
 * bytes across an interconnect.  The genuine data movement to/from the kernel
 * is the kernel's own m_axi traffic against DDR, which is inside kernel time
 * and is characterised separately (see results/nsweep_*).
 *
 * Reports, per transfer size:
 *   memcpy      host buffer -> mapped BO         (avoidable: write in place)
 *   sync sized  bo.sync(dir, size, 0)           (what a payload actually needs)
 *   sync full   bo.sync(dir)                    (what the current hosts do)
 */
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>

#include "mcoder.h"
#include "overhead.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

static std::string arg_of(int c, char **v, const char *f, const char *d) {
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], f)) return v[i + 1];
    return d;
}
static bool has_flag(int c, char **v, const char *f) {
    for (int i = 1; i < c; i++) if (!strcmp(v[i], f)) return true;
    return false;
}

int main(int argc, char **argv) {
    const std::string xclbin = arg_of(argc, argv, "-x", "mcoder.bin");
    const int iters = std::stoi(arg_of(argc, argv, "-n", "2000"));
    const bool csv = has_flag(argc, argv, "-q");

    auto device = xrt::device(0);
    auto uuid   = device.load_xclbin(xclbin);
    auto krnl   = xrt::kernel(device, uuid, "arith_kernel");

    auto bo_in  = xrt::bo(device, MC_MAX_IN,  krnl.group_id(0));
    auto bo_out = xrt::bo(device, MC_MAX_OUT, krnl.group_id(2));
    auto hin  = bo_in.map<mc_byte *>();
    auto hout = bo_out.map<mc_byte *>();
    (void)hout;

    std::vector<mc_byte> src(MC_MAX_IN);
    for (int i = 0; i < MC_MAX_IN; i++) src[i] = (mc_byte)(i & 0xFF);

    if (!csv) {
        std::cout << "host<->kernel transfer benchmark (NO kernel launches)\n"
                  << "BO sizes: in " << MC_MAX_IN << " B, out " << MC_MAX_OUT << " B\n"
                  << "iters per point: " << iters << "\n"
                  << "NOTE: shared-DDR SoC -- sync() is cache maintenance, not a bus copy.\n\n"
                  << "  size(B)   memcpy_us  syncTO_sized  syncFROM_sized"
                     "  syncTO_full  syncFROM_full   sized_MB/s\n";
    } else {
        std::cout << "size,memcpy_us,sync_to_sized_us,sync_from_sized_us,"
                     "sync_to_full_us,sync_from_full_us,sized_mbps\n";
    }

    for (int size = 256; size <= MC_MAX_IN; size <<= 2) {
        /* memcpy alone */
        auto t = ovh::Clock::now();
        for (int i = 0; i < iters; i++) memcpy(hin, src.data(), size);
        const double mc = ovh::us_since(t) / iters;

        /* sized sync, each direction */
        t = ovh::Clock::now();
        for (int i = 0; i < iters; i++) bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE, size, 0);
        const double st = ovh::us_since(t) / iters;

        t = ovh::Clock::now();
        for (int i = 0; i < iters; i++) bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE, size, 0);
        const double sf = ovh::us_since(t) / iters;

        /* whole-BO sync -- what the current hosts do, independent of payload */
        t = ovh::Clock::now();
        for (int i = 0; i < iters; i++) bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        const double ft = ovh::us_since(t) / iters;

        t = ovh::Clock::now();
        for (int i = 0; i < iters; i++) bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        const double ff = ovh::us_since(t) / iters;

        const double mbps = (st + sf) > 0 ? (size / ((st + sf) * 1e-6)) / 1e6 : 0;

        if (csv)
            std::cout << size << "," << mc << "," << st << "," << sf << ","
                      << ft << "," << ff << "," << mbps << "\n";
        else
            std::cout << "  " << size << "\t  " << mc << "\t   " << st
                      << "\t\t" << sf << "\t\t" << ft << "\t     " << ff
                      << "\t   " << mbps << "\n";
    }
    return 0;
}
