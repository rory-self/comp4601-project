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
  **4.9× faster than the ARM CPU** on the same board.
- **Iteration 2 — planned.** Make each *single* copy faster by pipelining it
  (harder; see §8). This would multiply on top of the replication.

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
| **Best HLS — 8-way replication (K=8)** | 12.9 M symbols/s | **3.7×** |
| **Best HLS — 16-way replication (K=16)** | 16.8 M symbols/s | **4.9×** |

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

---

## 8. Caveats & what iteration 2 will try

**Caveats (be honest about these):**
- Numbers are **workload-dependent**. Test input is compressible; near-random
  data emits more bits per symbol and runs slower.
- **Compression ratio degrades slightly with K** — each of the K chunks restarts
  its adaptive model and adds a small header. It's a throughput-vs-ratio trade you
  tune with K.

**Iteration 2 — pipeline the single stream (the hard part).** Right now each
stream is sequential. The way to speed up *one* stream is to restructure the coder
into a **flat state machine** that does exactly one unit of work per clock cycle
(either one interval update *or* one renormalisation step), with the variable-rate
output pushed into an `hls::stream` FIFO so it can't stall the pipeline. That
would make one stream ~5× faster, and because it **multiplies** with replication,
the combination could reach ~10–15× over the ARM CPU. It is genuinely hard and
uncertain (a naive pipeline pragma does not converge — see §3), which is why it's
a separate iteration.

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
```
