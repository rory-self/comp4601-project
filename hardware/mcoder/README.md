# mcoder — multiplier-free table-driven arithmetic coder (H.264 M-coder)

Same problem as [`replication_full`](../replication_full), same K-way chunking, but
the interval arithmetic is replaced by the **H.264/AVC CABAC "M-coder"**.
Probability becomes a **6-bit state index** instead of a 12-bit number, and the
`range × prob` multiply becomes a **384-byte ROM lookup** — so the design uses
**zero DSPs** and the multiply leaves the critical path entirely.

This is the fastest general-purpose design in the project, and it gets there while
compressing *better* than the coder it replaces.

| | |
|---|---|
| on fabric, kernel-only | **33.07 M sym/s** (30.24 ns/sym), lossless |
| vs the ARM A53 | **9.53×** (baseline 3.47 M sym/s) · **2.49×** `replication_full` |
| synthesis | II=1 on both stages, 0 timing violations, 36% LUT, **0 DSP** |
| post-route | WNS **+0.636 ns**, 0 failing endpoints of 71,515 |
| compression | **5.1% better** than `replication_full` at K=8 — not worse |
| image demo | 65536 → 40406 B (61.65%), pixel-perfect |

> **How to reproduce any of these:** [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) —
> the exact file, command, machine (workstation or board) and expected output for
> every number above.

> ⚠ **The presentation's M-coder bar (21.16 M sym/s) and its 6.09× are wrong** — not
> fabricated, but read from the *image demo*, whose timing includes host↔device
> transfer, while the other three bars are kernel-only. Measured consistently this
> design is **33.07 M sym/s and 9.53×**. `REPRODUCIBILITY.md` §8 has the detail.

## What is different from `replication_full`

| | replication_full | mcoder |
|---|---|---|
| interval split | `range × prob >> 12` — **48 DSPs** | ROM lookup — **0 DSPs** |
| renormalisation | data-dependent `while` loop | closed form, no loop |
| model update | `prob ± prob>>5` | state-transition ROM |
| context state | 12-bit probability | 7-bit (6-bit state + MPS) |
| cycles/byte (coder) | 83.7 | **8** |
| on fabric, kernel-only | 13.27 M sym/s | **33.07 M sym/s** |

**But the coder is no longer the bottleneck.** At K=8 it is 4,096 of 26,317 cycles —
**15.6%**. The other 84% is the byte-wide `m_axi` copies, which move the same total
bytes regardless of K. That is why K=16 fits after the LUTRAM change yet runs
*slower*, and it is where the next speedup is. See `REPRODUCIBILITY.md` §9.

## Layout
```
src/     mcoder.h, mcoder_tables.h   the engine + the H.264 tables (384 B)
         mcoder_enc.cpp              software encoder (hosts, tests, ARM baseline)
         mcoder_dec.cpp              the ONE decoder -- hosts and tests all link this
         mcoder_hls.cpp              the kernel: two dataflow stages, II=1
         arith_board.cpp             board top: arith_kernel(in,n,out,out_len)
         bench_host.cpp              throughput host  (kernel-only timing)
         demo_host.cpp               visual image demo (includes transfer)
test/    kernel_tb.cpp, board_tb.cpp C-sim / co-sim testbenches
         sw_bench.cpp                ARM software baseline
         compare_v5.cpp, v5_prof.*   side-by-side vs replication_full's coder
synth/   hls.cfg, board.cfg, link.cfg, sweep.sh (K + context-storage sweep)
bin/     arith.bin, arith_kernel.xo, bench_host_arm, demo_arm
data/    image.pgm, text_page.pgm, gen_text_page.py
results/ RAW LOGS ONLY -- onfabric_result.txt, arm_baseline.txt,
         rebuild_2026-08-08.txt, mcoder_hls_sweep.csv, mcoder_sweep.csv
```

## Quick start
```sh
source ../../env.sh              # from the repo root: source env.sh
../../run_on_board.sh mcoder     # deploys the prebuilt bitstream, runs, verifies
```

**`MC_KWAY` must match between the kernel and every host.** If they differ there is
no build error — the host misparses the container header and reports a round-trip
failure with no hint why. The shipped configuration is `MC_KWAY=8`.

`demo_arm -o image.mcz` writes the compressed stream as a real file and `-d` reads
it back, so compression and decompression are separately checkable steps rather than
one printed number. It also reports `CPU vs FPGA output: IDENTICAL bytes` — a
stronger claim than "it decoded", since a kernel with a subtly wrong context update
would still decode its own output but would not match the software model bit-for-bit.

**Only uncompressed input makes sense.** The coder compresses bytes and does not know
they are pixels; PNG/JPEG are already entropy-coded and it will *expand* them.

## Reference

The engine is the H.264/AVC CABAC "M-coder":

> D. Marpe and T. Wiegand, "A highly efficient multiplication-free binary
> arithmetic coder and its application in video coding," *ICIP 2003*,
> Barcelona, pp. 263–266. doi:10.1109/ICIP.2003.1246667

> D. Marpe, H. Schwarz, and T. Wiegand, "Context-based adaptive binary
> arithmetic coding in the H.264/AVC video compression standard," *IEEE TCSVT*,
> vol. 13, no. 7, pp. 620–636, July 2003. doi:10.1109/TCSVT.2003.815173

`src/mcoder_tables.h` is transcribed from the **standard**, not the papers:
ITU-T H.264 §9.3, Table 9-44 (`rangeTabLPS`) and Table 9-45 (state transitions).
The ICIP paper is included as
[`../../references/ieee03_multiplication_free.pdf`](../../references/ieee03_multiplication_free.pdf).

[`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) has the derivations, including the two that are provable
rather than empirical: the MPS renormalisation is always 0 or 1 bits, and the
8-step renormalisation recurrence has a closed form (one barrel shift plus a
prefix-AND).
