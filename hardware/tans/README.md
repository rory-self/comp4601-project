# tans — static table-driven coder ("tree method", tANS/FSE)

Build the entropy table **once** from a frequency table the input files share, then
code a whole **byte per state transition** (`state = STATE[...]`) — no multiply, no
per-symbol model update. Kernel = 4 SIMD lanes + 64-bit AXI.

This is the fastest design in the project by a wide margin, and the only one where
we measured energy.

| | |
|---|---|
| on fabric, 1 CU | **147.9 M sym/s**, lossless, ratio 84.08% |
| on fabric, 2 CUs | **286.6 M sym/s** — 1.95× scaling |
| vs the ARM A53 | **2.63×** → **5.10×** (baseline 56.25 M sym/s) |
| energy (INA260 sensor) | **23.3 vs 61.6 nJ/byte = 2.64× less** |
| synthesis | II=2, ~225 MHz, 16% LUT, **0 DSP** |

> **How to reproduce any of these:** [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) —
> the exact file, command, machine (workstation or board) and expected output for
> every number above, including the power-sampling method.

> ⚠ **Do not compare this 2.63× with the other designs' speedups.** The baseline
> here is **56.25 M sym/s**, ~16× higher than theirs, because tANS is byte-wise and
> they are bit-wise. Compare throughput (147.9 vs 33.07 M sym/s), never ratios.
> Compression ratios are not comparable at all — different input files.

## Scope — read this before quoting any ratio

The table in `src/tans_table.h` is baked from **one** distribution. tANS is lossless
on any input but only *compresses* data matching that table: it expands an aarch64
ELF to 143% and a PDF to 124%. Every figure above is on `data/file0..3.bin`, the
workload class the design targets.

That is the whole premise, not a limitation we forgot to fix: precompute the bounds
once, amortise them across many files that share a distribution, then each file is
pure table traversal. [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) measures the cost of that trade
— it **ties** the adaptive coder on stationary data (−0.36%) and loses 24–40% on
natural images.

## How it got fast — it was I/O all along

| version | throughput | blocker removed |
|---|---:|---|
| K-way, shared table ROM | 28.2 M/s | — (lanes serialised: one ROM read port) |
| + SIMD lockstep, per-lane tables | 35.8 M/s | lanes actually parallel |
| + **64-bit aligned AXI** | **194.0 M/s** | **I/O was 90% of runtime** |
| on real fabric | 147.9 M/s | minus ~24 µs/call XRT overhead |

**No algorithm changed across those rows.** Widening the AXI port bought **5.4×** —
the single largest win anywhere in this project. The identical change made
[`../interleaved`](../interleaved) **32.9% slower**, because that design gathers
unaligned words at chunk boundaries and this one reads aligned words sequentially.

**DATAFLOW does not help here and never will:** measured 25,461 vs 16,851 cycles,
50% slower. tANS encodes *in reverse*, so the coder cannot start until the whole
block has arrived — Load can never overlap the encoder. That is a property of ANS,
not a tuning failure.

## Layout
```
src/     arith_tans.cpp   the kernel (also the board top: arith_kernel)
         tans_table.h     the baked static table
         tans.h           table construction + software coder
         demo_host.cpp    SW-vs-HW host (also -m sw|hw energy modes)
         multi_host.cpp   1 CU vs 2 CUs scaling test
test/    kernel_tb.cpp    co-simulation testbench
         tans_bench.cpp   software round-trip + ratio + CPU speed
synth/   hls.cfg          C-synthesis / co-sim / .xo
         link.cfg         1 compute unit @200 MHz
         link_multi.cfg   2 compute units on separate HP ports
bin/     arith.xclbin, arith_multi.xclbin, demo_host_arm, multi_host_arm
data/    file0..3.bin     four files drawn from ONE shared distribution
results/ RAW LOGS ONLY -- onfabric_result.txt, energy_measurement.txt
```

## Quick start
```sh
source ../../env.sh              # from the repo root: source env.sh
../../run_on_board.sh tans       # 1 compute unit
../../run_on_board.sh multi      # 2 compute units, scaling test
```

Full build path is in [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md).

## Note on the table

Every symbol must get at least one slot (**Laplace smoothing**). Without it,
encoding a byte that never appeared indexes a table entry that was never built and
the coder **segfaults**. Smoothing costs 0.088% ratio and makes the coder total.
