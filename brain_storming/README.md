# Hardware-Accelerated Arithmetic Coding

COMP4601 project — accelerating lossless data compression (**adaptive arithmetic
coding**) on the AMD/Xilinx **KV260** FPGA board using **High-Level Synthesis
(HLS)**. This folder holds three implementations (a naive hardware version, an
optimised hardware version, and a software reference) and the measured results
comparing them.

This README is written to be understandable from scratch — if you have never
seen arithmetic coding or FPGAs before, start at the top and it should all make
sense.

---

## 0. Iteration status (read this first)

- **Iteration 1 — COMPLETE (this folder).** We get our speedup by running **many
  copies of the coder in parallel** ("multi-stream replication"). Result:
  **up to 4.9× faster than the ARM CPU** on the same board.
- **Confirmed on real hardware.** The kernel was built into a bitstream and run on
  the KV260's FPGA fabric (not just simulated). The K=8 design measured **13.29 M
  symbols/s = 3.84× the ARM CPU**, matching the co-simulation prediction within
  ~3% (§6). There is also a **live image-compression demo** — CPU vs FPGA on the
  board — see `demo/` and §7.
- **Iteration 2 — HLS experiments complete.** A time-interleaved design raises
  each stream's recurrence distance and achieves aggregate **II=1** with shared
  arithmetic hardware. The best resource-efficient variant reaches **22.30 M
  symbols/s = 6.45× ARM** in RTL co-simulation using only 21% LUT, while
  preserving the original 255-context model. See
  [`interleaved_hls/RESULTS.md`](interleaved_hls/RESULTS.md). A separate
  near-device-filling K=31 experiment reaches 28.20 M symbols/s at 95% LUT but
  weakens the probability model; see [`max_hls/RESULTS.md`](max_hls/RESULTS.md).
  Neither iteration-2 design has yet been placed/routed or board-validated.

---

## 1. What is arithmetic coding? (the algorithm)

Compression means representing data in fewer bits. **Arithmetic coding** is a
lossless compression method (you get the exact original data back) that is very
efficient — it beats simpler schemes like Huffman coding.

The idea: represent the **entire message as a single number** between 0 and 1.

- Start with the interval `[0, 1)`.
- Split it into sub-slices, one per possible symbol, sized by how *likely* each
  symbol is. Common symbols get big slices, rare symbols get small slices.
- Read the first input symbol → zoom into its slice. That slice becomes your new
  interval.
- Split *that* slice the same way, read the next symbol, zoom in again.
- Repeat for every symbol. The interval keeps shrinking.
- At the end, output any single number inside the final tiny interval. That one
  number *is* the compressed message.

Why it compresses: a likely symbol barely shrinks the interval (cheap, few
bits); an unlikely symbol shrinks it a lot (expensive, more bits). Over the whole
message this reaches the theoretical minimum number of bits (the "entropy").

reference: https://medium.com/@nit4642/arithmetic-coding-f2a7559c0dbd

**"Adaptive"** means the model of symbol probabilities isn't fixed up front — it
*learns* as it reads the data (e.g. after seeing lots of the letter 'e', it makes
'e' 's slice bigger). This compresses real data better because it adapts to
whatever you actually feed it.

To turn the shrinking interval into output bits, the coder uses
**renormalisation**: whenever the interval gets small enough that its top bit is
settled, it emits that bit and rescales the interval back up. So output bits
dribble out as you go, a variable number per symbol.

---

## 2. What does "hardware acceleration on an FPGA" mean?

A normal program runs on a **CPU** (here, the board's ARM processor) — it does
one thing at a time, very fast (~1.3 billion steps/second).

An **FPGA** is a chip whose digital logic you can *configure* into a custom
circuit. Instead of running instructions, you build hardware that does exactly
your computation. FPGAs run at a slower clock (here 200 MHz = 0.2 billion
steps/second) **but** they can do **many operations physically at the same time**
(spatial parallelism) and overlap work in a pipeline. If your problem allows lots
of parallel work, the FPGA wins despite the slower clock. If it doesn't, the FPGA
*loses* to the CPU — this turns out to be the whole story here.

**HLS (High-Level Synthesis)** lets us write the hardware in C++ instead of
low-level hardware description languages. We add `#pragma HLS ...` hints (pipeline
this loop, unroll that one, split this array into separate memories) and the tool
generates the circuit. We measure a design two ways:
- **Latency / II** — how many clock cycles it takes. *II* (initiation interval)
  is how many cycles between starting one piece of work and the next; II=1 means
  a new one every cycle (ideal).
- **Resources** — how much of the chip it uses (LUTs = logic, DSPs = multipliers,
  BRAM = memory blocks, FFs = registers).

---

## 3. The core problem: why arithmetic coding fights acceleration

Here is the key obstacle, and everything in this project follows from it.

Encoding symbol **N** needs the interval (`low`/`high`) produced by symbol
**N-1**. You literally cannot compute symbol N until symbol N-1 is done. This is a
**sequential recurrence** — a dependency chain with no way around it.

Consequences:
1. **You can't parallelise a single stream.** Splitting the work of one message
   across parallel hardware is impossible, because every step waits on the one
   before it.
2. **You can't easily pipeline it either.** Pipelining overlaps consecutive steps
   in time, but the renormalisation emits a *variable number of output bits per
   symbol*, which a fixed-rate pipeline can't absorb. (We confirmed this: forcing
   the compiler to pipeline the coding loop ran for >14 minutes without producing
   a result.)

So a straightforward FPGA port is doomed to be *slower* than the CPU: same
sequential work, but at 200 MHz instead of 1.3 GHz. **That is exactly what we
measure for the naive version.** The interesting engineering is how to get around
this wall.

---

## 4. The three implementations in this folder

### `naive_hls/` — the unoptimised baseline
A textbook arithmetic coder ported straight to HLS: a large symbol alphabet, a
**division** per symbol (expensive in hardware), a frequency model rebuilt with
O(N) work each symbol, and inputs read one-by-one from off-chip memory. It is
correct but ~15× *slower* than the CPU — the honest starting point that shows the
problem from §3.

### `best_hls/` — the optimised, accelerated design
Two layers of optimisation:

**Layer A — make one stream as lean as possible.** We switched to a **binary
range coder** (LZMA-style): it codes one *bit* at a time with a probability model,
which lets us replace the costly division with a **bit-shift** (a power-of-two
total), and use a tiny adaptive bit-model. We also **burst-load the input** into
on-chip memory instead of trickling it from DRAM. This makes a single stream
several times leaner — but it is *still sequential*, so it's still roughly on par
with / slightly behind the CPU. Necessary, but not sufficient.

**Layer B — replication (the actual win).** Since we can't parallelise *inside* a
stream, we split the input into **K independent chunks** and run **K complete
coders side by side**, each compressing its own chunk. K coders finish in roughly
the time of one → up to K× the throughput. This is the standard way entropy
coders are accelerated on FPGAs. `arith5.cpp` is parameterised by `-DKWAY=K`.

### `software/` — the fair CPU baseline
The *same* binary coder compiled to run on the CPU (single stream, `KWAY=1`).
This is what we must beat, and we measured it on the actual ARM chip on the board.

---

## 5. Why replication wins (the key insight in one paragraph)

A CPU is one fast sequential engine. Our coder is a sequential problem, so on one
stream the CPU beats the FPGA (faster clock, and neither can parallelise the
recurrence). **But an FPGA can hold many coder circuits at once.** By compressing
K independent chunks on K parallel coders, the FPGA does K messages' worth of
work simultaneously — something the single CPU cannot do without extra cores. We
trade a little compression ratio (each chunk restarts its model, plus a small
header) for K× throughput. That parallel-across-streams throughput is where the
FPGA overtakes the CPU.

---

## 6. Results (all measured; 200 MHz FPGA, compressible input)

| implementation | throughput | vs ARM CPU |
|---|---|---|
| **Software — ARM Cortex-A53 (measured on board)** | 3.46 M symbols/s | 1.0× (the baseline to beat) |
| Software — x86 laptop reference (for context) | 23.9 M symbols/s | 6.9× |
| Naive HLS — single stream | 0.29–2.4 M symbols/s | **0.08–0.69× (SLOWER!)** |
| **Best HLS — K=8, MEASURED ON FPGA FABRIC** | **13.29 M symbols/s** | **3.84×** |
| Best HLS — K=8, co-simulation (predicted) | 12.9 M symbols/s | 3.7× |
| Best HLS — K=16, co-simulation | 16.8 M symbols/s | 4.9× |

### Iteration-2 comparison (HLS synthesis + RTL co-simulation)

These designs have not yet been placed/routed or tested on the board:

| design | clock | throughput | vs ARM | LUT | probability model |
|---|---:|---:|---:|---:|---|
| Original physically replicated K=16 | 200 MHz | 16.8 M symbols/s | 4.9× | 71% | 255-context tree |
| Interleaved, 4 shared engines | 200 MHz | 20.68 M symbols/s | 5.98× | 18% | 255-context tree |
| **Interleaved, 8 shared engines** | **150 MHz** | **22.30 M symbols/s** | **6.45×** | **21%** | 255-context tree |
| Lean physically replicated K=31 | 200 MHz | 28.20 M symbols/s | 8.15× | 95% | reduced 8-probability model |

The time-interleaved architecture is the main iteration-2 result. Independent
coder states rotate through shared arithmetic pipelines, increasing the
recurrence distance enough for HLS to schedule the central loop at aggregate
**II=1**. It improves throughput per LUT substantially without weakening the
model or changing the K=16 compression ratio. The K=31 result is an absolute
HLS-throughput experiment, but it depends on nearly filling this specific FPGA
and sacrifices compression efficiency.

Memory access was tested separately. The compatible byte-pointer version already
inferred AXI bursts and completed the 4095-symbol four-engine test in **39,606
cycles**. Explicitly gathering a 64-bit input into unaligned per-chunk words took
**52,635 cycles**, used more LUT/BRAM, and was 32.9% slower. Input staging plus
header/output assembly account for at most about 15% of the recommended design's
latency, so arithmetic work remains the main bottleneck. Details and reproducible
configs are in [`interleaved_hls/RESULTS.md`](interleaved_hls/RESULTS.md).

**On-fabric validation (the important confirmation):** the K=8 kernel was
synthesised (closes timing at 200 MHz, 3.65 ns), linked into a bitstream, loaded
onto the PL, and driven by an XRT host. Measured: **13.29 M symbols/s (3.84× the
ARM), losslessly** — the host decoded the *board's* compressed output and it
matched the input exactly. Crucially, the real-fabric number (13.29) and the
co-sim number (12.9) **agree within ~3%**: because the kernel compresses a whole
4 KB buffer per call, the XRT launch overhead amortises to near-zero per symbol.
(Raw log: `board/onfabric_result.txt`.) K=16 was left as a co-sim result to keep
the first fabric build low-risk; it scales the same way (~4.9×).

How to read this:
- The **naive** FPGA version is genuinely *slower* than the CPU — proof of the §3
  wall (sequential work at a slower clock).
- **Replication crosses the line into real speedup:** 16 parallel coders reach
  **4.9× the ARM CPU's throughput.**
- The ARM number is **measured on the board** (Cortex-A53 @ 1.334 GHz): 288.68
  ns/symbol → 3.46 M symbols/s (raw log in `results/arm_software_board.txt`),
  timed with `std::chrono` around only the encode call.

**How well does replication scale?** (measured, `results/sweep*.csv`)

| K (parallel coders) | speedup vs 1 stream | FPGA logic used (LUT) |
|---|---|---|
| 8 | 5.38× | 37 % |
| 16 | 7.04× | 71 % |

It doesn't scale perfectly linearly (8 coders give 5.4×, not 8×) because of fixed
per-chunk setup that doesn't parallelise — this is **Amdahl's law**, and it eases
on larger inputs. Beyond ~K=16 the chip runs out of logic (LUTs), so that's the
practical ceiling for this approach.

---

## 7. How to build and run (each part is self-contained)

Correctness + speed, no FPGA tools needed (plain `g++`):
```
# naive coder — verify it's lossless
cd naive_hls && g++ -O2 -Wno-unknown-pragmas arith.cpp arith_test.cpp -o t && ./t

# best coder — 16-way, verify lossless
cd best_hls && g++ -O2 -Wno-unknown-pragmas -DKWAY=16 arith5.cpp arith5_test.cpp -o t && ./t

# software baseline throughput (single stream)
cd software && g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 arith5.cpp sw_bench.cpp -o sw && ./sw
```
Every coder prints `round-trip=OK` / `PASS` — i.e. decode(encode(x)) == x, so the
compression is verified lossless.

FPGA synthesis / co-simulation (needs Vitis 2025.2 + the free Vitis HLS licence):
configs are `naive_hls/hls_config.cfg` and `best_hls/hls_config5.cfg`; the scripts
`best_hls/sweep.sh` and `sweep_big.sh` reproduce the K-sweeps via
`vitis-run --mode hls --csim|--cosim` and `v++ -c --mode hls`.

### Running the real kernel on the board (`board/`)

The `board/` folder has the on-fabric kernel (K=8), an XRT host, and a **prebuilt
bitstream** (`arith.bin`) so you can run without a 10-min Vivado build.

```
# on the board (after loading arith.bin via xmutil loadapp arith):
./arith_host_arm -x arith.bin -N 4095 -n 2000
```
Cross-compiling the host yourself (aarch64, against the XRT sysroot):
```
aarch64-linux-gnu-g++ -O3 -std=c++17 --sysroot=<SYSROOT> \
  -I<SYSROOT>/usr/include -I<SYSROOT>/usr/include/xrt \
  host.cpp arith5.cpp -DKWAY=8 -o arith_host_arm \
  -L<SYSROOT>/usr/lib -lxrt_coreutil -lpthread -lrt -ldl -luuid
```
Rebuilding the bitstream from source: `vitis-run --mode hls --package` with
`hls_board.cfg` to get `arith_kernel.xo`, then `v++ --link --target hw` with
`link.cfg` against the `kv260_custom` platform → `arith.xclbin`.

### 🖼️  Live image-compression demo (`demo/`) — the visual demo

Compresses a real image on **both** the ARM CPU and the FPGA, decompresses it,
checks the reconstruction is pixel-perfect, and prints an ASCII preview + the
speedup — all in the terminal, all on the board.

```
cd demo
./run_demo.sh <BOARD_IP>        # deploys, loads the bitstream, runs, pulls the result back
```
(or, if already deployed, on the board: `./demo_arm -i image.pgm -x arith.bin`)

Example output (256×256 image):
```
original size    : 65,536 bytes
compressed size  : 42,661 bytes   (65.1% of original)
lossless         : YES — reconstructed == original, pixel-perfect
CPU vs FPGA out  : identical bytes
ARM CPU compress : 21,908 us   (2.99 MB/s)
FPGA    compress :  6,071 us   (10.79 MB/s)
SPEEDUP          : 3.61x
```
`image.png` and `reconstructed.png` are the before/after (identical, since it's
lossless). The demo also prints the image as ASCII art before and after.

---

## 8. Caveats and iteration-2 findings

**Caveats (be honest about these):**
- Numbers are **workload-dependent**. Test input is compressible; near-random
  data emits more bits per symbol and runs slower.
- **Compression ratio degrades slightly with K** — each of the K chunks restarts
  its adaptive model and adds a small header. It's a throughput-vs-ratio trade you
  tune with K.
- End-to-end acceleration still changes with image size because model reset,
  headers, transfers, and launch time are fixed costs. The iteration-2 comparison
  therefore uses the same 4095-symbol workload for every design.
- Resource estimates are device-specific, but the interleaved design reduces
  that dependence: its best candidate uses only 21% LUT rather than obtaining
  speed by filling the KV260.

**What iteration 2 established:**
- A flat single-state FSM is not enough: HLS scheduled its main loop at **II=25**
  because the interval recurrence still has distance one.
- Rotating independent states through a shared datapath changes that recurrence
  distance and achieves **II=1**.
- Four shared engines are the safer 200 MHz candidate; eight engines give the
  best theoretical resource-efficient throughput at 150 MHz.
- Merely widening the existing contiguous-chunk memory layout is counterproductive
  because its unaligned gather cost exceeds the saved AXI transactions.
- Streaming overlap could improve the current workload by at most roughly 1.18×
  and would require a striped host layout or multiple AXI channels, so it is not
  the central source of acceleration.

---

## File map

```
naive_hls/   arith.cpp, arith.h, arith_test.cpp   unoptimised HLS coder + its testbench
             hls_config.cfg                         Vitis HLS build config
best_hls/    arith5.cpp, arith5_test.cpp           optimised coder (binary + K-way replication)
             arith3.h                               shared binary range-coder core
             hls_config5.cfg, sweep*.sh             build config + K-sweep reproduction scripts
software/    sw_bench.cpp, arith5.cpp, arith3.h     CPU throughput benchmark (same coder, K=1)
results/     sweep_results.csv (256-sym)            measured K-sweep, small input
             sweep_big_results.csv (1024-sym)       measured K-sweep, larger input (better scaling)
             arm_software_board.txt                 raw ARM Cortex-A53 measurement
board/       arith_board.cpp                        board top-level kernel (K=8, adds out_len port)
             host.cpp                               XRT throughput host (chrono around kernel only)
             hls_board.cfg, link.cfg                HLS package + v++ link configs
             arith.bin                              prebuilt bitstream (loadable with xmutil)
             onfabric_result.txt                    raw on-fabric measurement (13.29 M sym/s)
             arith5.cpp, arith3.h                   coder sources (for standalone build)
demo/        demo_host.cpp                          image demo: CPU vs FPGA compress + reconstruct
             run_demo.sh                            deploy + run the demo on the board
             demo_arm                               prebuilt aarch64 demo binary
             image.pgm/.png, reconstructed.png      the test image + its (identical) reconstruction
             arith5.cpp, arith3.h                   coder sources (for standalone build)
interleaved_hls/
             arith_interleaved.cpp/.h                shared II=1 time-interleaved coder
             interleaved_test.cpp                    lossless software/RTL testbench
             interleaved_g4.cfg                      4-engine, 200 MHz board candidate
             interleaved_g8_150.cfg                  8-engine, 150 MHz throughput candidate
             interleaved_g4_wide.cfg                 rejected wide-input comparison
             RESULTS.md                              full synthesis/co-simulation findings
max_hls/    arith_max.cpp/.h                         lean near-device-filling K=31 experiment
             arith_max_test.cpp, k31_max.cfg         lossless test + HLS configuration
             RESULTS.md                              limits, compression cost, and results
```
