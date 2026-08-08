// overhead.h -- shared setup/transfer accounting for every on-board host.
//
// Every "FPGA is Nx faster" figure in this project was measured with chrono
// around the kernel call alone.  That is the right boundary for judging the
// datapath and the wrong one for judging whether offloading was worth doing:
// before a single symbol is coded the host must open the device, push a ~7.8 MB
// bitstream into the PL, resolve the compute unit, allocate and mmap DMA
// buffers, and copy the payload across the AXI bus -- and copy the result back.
//
// This header does not replace the existing numbers.  It records the costs the
// old boundary excluded and reports the speedup at three boundaries:
//
//   1. compute only          the kernel vs the ARM coder      (what we had)
//   2. + host<->device DMA   what offload actually costs per payload
//   3. + one-time setup      end-to-end, one process, one payload
//
// plus the break-even payload: the amount of data a single process must push
// through before the one-time setup has paid for itself.  Boundary 1 flatters
// the FPGA, boundary 3 flatters the CPU, and the honest answer is "depends how
// much data you have" -- which is exactly what break-even states.
//
// Note on `xclbin_load`: run_on_board.sh programs the PL with `xmutil loadapp`
// before the host starts, so load_xclbin() is measured against firmware that is
// already resident.  It is XRT's load path (read the file, match the UUID, map
// the CUs), not a cold FPGA configuration -- i.e. a lower bound on setup.
//
// Header-only.  Add -I../../common to the host cross-compile.

#ifndef HW_OVERHEAD_H
#define HW_OVERHEAD_H

#include <chrono>
#include <cstdio>
#include <initializer_list>

namespace ovh {

using Clock = std::chrono::high_resolution_clock;

inline double us_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
}

// RAII stopwatch: adds its own lifetime, in microseconds, to `sink`.
class Scope {
public:
    explicit Scope(double& sink) : sink_(sink), t0_(Clock::now()) {}
    ~Scope() { sink_ += us_since(t0_); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    double& sink_;
    Clock::time_point t0_;
};

struct Breakdown {
    // ---- one-time FPGA setup: paid once per process, whatever the payload ----
    double device_open = 0;   // xrt::device()       open the device, attach to XRT
    double xclbin_load = 0;   // load_xclbin()       read the .xclbin, program/match the PL
    double kernel_open = 0;   // xrt::kernel()       resolve the CU and its argument map
    double bo_alloc    = 0;   // xrt::bo() + map()   allocate and mmap the DMA buffers

    // ---- host-side work BOTH paths pay (not an offload cost) ----
    double input_read  = 0;   // pulling the payload off disk

    // ---- software-side one-time setup (entropy tables etc.; 0 for adaptive coders) ----
    double sw_setup    = 0;

    // ---- per-payload cost of using the accelerator ----
    double h2d         = 0;   // memcpy into the BO + sync TO device
    double d2h         = 0;   // sync FROM device (+ copy the result out)
    double hw_compute  = 0;   // kernel enqueue + wait
    double sw_compute  = 0;   // the same payload on the ARM

    long bytes = 0;           // payload the four figures above refer to
    const char* unit = "sym"; // symbol name for throughput lines

    double hw_setup()   const { return device_open + xclbin_load + kernel_open + bo_alloc; }
    double hw_move()    const { return h2d + d2h; }
    double hw_offload() const { return hw_compute + hw_move(); }
    double hw_e2e()     const { return hw_setup() + input_read + hw_offload(); }
    double sw_e2e()     const { return sw_setup + input_read + sw_compute; }
};

namespace detail {

inline void row(const char* label, double us, double total) {
    if (total > 0) std::printf("  %-30s %12.1f us   %5.1f%%\n", label, us, 100.0 * us / total);
    else           std::printf("  %-30s %12.1f us\n", label, us);
}

// Bytes with a readable magnitude, so break-even figures are legible.
inline void print_bytes(double b) {
    if      (b >= 1e9) std::printf("%.2f GB", b / 1e9);
    else if (b >= 1e6) std::printf("%.2f MB", b / 1e6);
    else if (b >= 1e3) std::printf("%.2f kB", b / 1e3);
    else               std::printf("%.0f B",  b);
}

} // namespace detail

// Where the wall-clock of one end-to-end FPGA run actually goes.
inline void report_overhead(const Breakdown& b) {
    const double e2e = b.hw_e2e();
    std::printf("\n=== overhead breakdown (one process, one payload of %ld bytes) ===\n", b.bytes);
    std::printf("  ONE-TIME SETUP (independent of payload size)\n");
    detail::row("open device",               b.device_open, e2e);
    detail::row("load xclbin -> program PL",  b.xclbin_load, e2e);
    detail::row("create kernel handle",       b.kernel_open, e2e);
    detail::row("allocate + map DMA buffers", b.bo_alloc,    e2e);
    detail::row("  subtotal: FPGA setup",     b.hw_setup(),  e2e);
    std::printf("  PER-PAYLOAD\n");
    if (b.input_read > 0) detail::row("read input from disk (both)", b.input_read, e2e);
    detail::row("host -> device (memcpy+DMA)", b.h2d,        e2e);
    detail::row("kernel enqueue + wait",       b.hw_compute, e2e);
    detail::row("device -> host (DMA)",        b.d2h,        e2e);
    detail::row("  subtotal: offload",         b.hw_offload(), e2e);
    detail::row("TOTAL end-to-end (FPGA)",     e2e,          e2e);
    if (b.hw_offload() > 0)
        std::printf("  data movement is %.1f%% of the offload cost; the kernel is %.1f%%\n",
                    100.0 * b.hw_move() / b.hw_offload(),
                    100.0 * b.hw_compute / b.hw_offload());
}

// The same workload at all three boundaries, plus break-even.
// Requires sw_compute and bytes to be set.
inline void report_speedup(const Breakdown& b) {
    if (b.sw_compute <= 0 || b.bytes <= 0) return;

    std::printf("\n=== speedup at three measurement boundaries ===\n");
    std::printf("  %-34s %12s %12s %10s\n", "boundary", "SW (us)", "HW (us)", "speedup");
    std::printf("  %-34s %12.1f %12.1f %9.2fx\n", "1. compute only",
                b.sw_compute, b.hw_compute, b.sw_compute / b.hw_compute);
    std::printf("  %-34s %12.1f %12.1f %9.2fx\n", "2. + host<->device transfer",
                b.sw_compute, b.hw_offload(), b.sw_compute / b.hw_offload());
    std::printf("  %-34s %12.1f %12.1f %9.2fx\n", "3. + one-time setup (end-to-end)",
                b.sw_e2e(), b.hw_e2e(), b.sw_e2e() / b.hw_e2e());
    std::printf("     (1) is the datapath figure quoted elsewhere; (3) is what a user\n"
                "     running this once on this much data actually experiences.\n");

    // Marginal rates, us per byte -- setup does not scale with the payload, so
    // the FPGA overtakes the CPU once (s - h) * bytes exceeds the setup cost.
    const double s = b.sw_compute / b.bytes;
    const double h = b.hw_offload() / b.bytes;
    std::printf("\n  marginal cost   SW %.4f us/byte   HW %.4f us/byte (kernel + DMA)\n", s, h);
    if (s > h) {
        const double be = b.hw_setup() / (s - h);
        std::printf("  break-even      ");
        detail::print_bytes(be);
        std::printf("  in one process (%.1fx this run's payload)\n", be / b.bytes);
        std::printf("                  below that the ARM finishes first; above it the\n"
                    "                  FPGA wins by %.4f us for every further byte\n", s - h);
    } else {
        std::printf("  break-even      never -- with transfers included the FPGA's marginal\n"
                    "                  rate is not better than the ARM's, so setup is never repaid\n");
    }
}

// Effective throughput once the one-time setup is amortised over a growing
// number of payloads. For hosts with no software baseline to compare against.
inline void report_amortised(const Breakdown& b) {
    if (b.bytes <= 0 || b.hw_offload() <= 0) return;
    const double steady = b.bytes / b.hw_offload();          // M sym/s, setup excluded
    std::printf("\n=== setup amortisation (effective throughput incl. one-time setup) ===\n");
    std::printf("  %10s %14s %16s %12s\n", "payloads", "total bytes", "M sym/s", "of steady");
    for (long n : {1L, 10L, 100L, 1000L, 10000L, 100000L}) {
        const double t   = b.hw_setup() + n * b.hw_offload();
        const double eff = (double)n * b.bytes / t;
        std::printf("  %10ld %14ld %16.3f %11.1f%%\n", n, n * b.bytes, eff, 100.0 * eff / steady);
    }
    std::printf("  %10s %14s %16.3f %11.1f%%\n", "steady", "(setup->0)", steady, 100.0);
    std::printf("  one-time setup = %.1f us = the time to code %.0f bytes at steady state\n",
                b.hw_setup(), b.hw_setup() * steady);
}

} // namespace ovh

#endif // HW_OVERHEAD_H
