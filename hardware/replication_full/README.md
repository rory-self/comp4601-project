# replication_full — K-way replicated adaptive arithmetic coder

Binary range coder (shift, not divide) over a 255-context adaptive bit-tree,
replicated **K** ways. The interval recurrence makes one stream sequential, so
parallelism comes from coding K independent chunks side by side (`-DKWAY=K`).

This was the first design on fabric and is the project's reference point — **the
ARM baseline for every other design comes from here** (`test/sw_bench.cpp`, KWAY=1).

| | |
|---|---|
| on fabric, kernel-only | **13.27 M sym/s** (75.37 ns/sym), lossless |
| vs the ARM A53 | **3.82×** (baseline 3.47 M sym/s) |
| synthesis | II=1, Fmax 273.97 MHz, 37% LUT, **48 DSP (3%)** |
| image demo | 65536 → 42661 B (65.10%), pixel-perfect |

> **How to reproduce any of these:** [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) —
> the exact file, command, machine (workstation or board) and expected output for
> every number above.

The 48 DSPs are 8 lanes × the `range × prob` interval multiply. That multiply is
what [`../interleaved`](../interleaved) shares between lanes and what
[`../mcoder`](../mcoder) removes altogether.

## Layout
```
src/     arith5.cpp, arith3.h   the coder (KWAY-parameterised)
         arith_board.cpp        board top: arith_kernel(in,n,out,out_len)
         bench_host.cpp         throughput host  (kernel-only timing)
         demo_host.cpp          visual image demo (CPU vs FPGA, includes transfer)
test/    kernel_tb.cpp, board_tb.cpp, sw_bench.cpp
synth/   hls.cfg, board.cfg, link.cfg, sweep.sh (K-sweep)
bin/     arith.bin, bench_host_arm, demo_arm
data/    image.pgm, img_smooth.pgm, img_noise.pgm
results/ RAW LOGS ONLY -- onfabric_result.txt, arm_baseline.txt, image_demo.txt,
         sweep_results.csv, sweep_big_results.csv
```

## Quick start
```sh
source ../../env.sh          # from the repo root: source env.sh
../../run_on_board.sh rep    # deploys the prebuilt bitstream, runs, verifies lossless
```

A prebuilt `bin/arith.bin` is committed so the demo runs without a 20–40 min
rebuild. Full build path — software test, synthesis, co-sim, bitstream, host
cross-compile — is in [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md).

## Scope

A lean K=31 variant (28.20 M/s at 95% LUT) was explored and set aside: **co-simulation
only, never built, never board-validated**, and at 95% LUT it is over the practical
placement ceiling. The full reasoning is in
[`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) §9. Treat it as theory.
