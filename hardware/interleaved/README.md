# interleaved — C-slow time-interleaved arithmetic coder

One shared arithmetic datapath serves `LANES=16` independent coder states
round-robin. A lane is revisited only every `LANES/GROUPS` cycles, so the
loop-carried interval recurrence gets that many cycles of slack — enough for HLS to
schedule the coding loop at aggregate **II=1** *and* raise the clock.
`GROUPS=4` is the shipped configuration (recurrence distance 4).

The point of this design is **throughput per LUT**: it keeps the full 255-context
adaptive model at 19% LUT, where equivalent replication needs 71%.

| | |
|---|---|
| on fabric, kernel-only | **11.67 M sym/s**, lossless |
| vs the ARM A53 | **3.36×** (baseline 3.47 M sym/s) |
| co-simulation, kernel-only | 20.68 M sym/s (39,623 cycles / 4 KB) |
| Fmax, board top | **273.97 MHz** — the coder alone reaches 342.47; the AXI wrapper is the critical path |
| area | 23,249 LUT (19%), 6 DSP, 50 BRAM (17%) |

> **How to reproduce any of these:** [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) —
> the exact file, command, machine (workstation or board) and expected output for
> every number above, plus the overhead budget and the negative results.

### Why fabric (11.67) sits below co-simulation (20.68)

Two measured causes, no mystery:

1. **The platform's clocks are quantised** to 100/200/400 MHz, so the 273.97 MHz
   Fmax still runs at **200** — the headroom is simply discarded.
2. **This kernel's `m_axi` does not burst** (HLS warns "inferred burst reverted").
   Cosim models AXI as a fixed 64-cycle latency and hides that; on real DDR it costs
   **~1.46×**.

Subtracting the independently-measured ~62 µs XRT overhead per call leaves 289 µs of
kernel time against cosim's 198 µs — that ratio *is* the 1.46×.

> ⚠ **The "31 M sym/s" figure quoted in our presentation is a projection, not a
> measurement.** It is the cosim cycle count scaled to Fmax, and it lands anywhere
> between 28.3 and 35.4 M/s depending on which Fmax you use. Only **20.68** (cosim)
> and **11.67** (fabric) are measured.

## Layout
```
src/     arith_interleaved.cpp/.h  the coder (exact-width ap_uint, optional REG_MUL)
         arith_board.cpp           board top: arith_kernel(in,n,out,out_len)
         bench_host.cpp            throughput host  (kernel-only timing)
         demo_host.cpp             live SW-vs-HW host
test/    kernel_tb.cpp, board_tb.cpp
synth/   hls.cfg    standalone coder (C-synthesis + co-sim)
         board.cfg  board top -> .xo
         link.cfg   1 compute unit @200 MHz
bin/     arith.xclbin, bench_host_arm, demo_host_arm
results/ RAW LOGS ONLY -- onfabric_result.txt, synth_2026-08-08.txt,
         cosim_2026-08-08.txt
```

## Quick start
```sh
source ../../env.sh                  # from the repo root: source env.sh
../../run_on_board.sh interleaved    # deploys the prebuilt bitstream, runs, verifies
```

Full build path is in [`REPRODUCIBILITY.md`](REPRODUCIBILITY.md). The mechanism —
why interleaving breaks the recurrence at all — is in [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md).

## How it compares with the other ways of getting parallelism

| design | throughput | LUT | model |
|---|---:|---:|---|
| physically replicated K=16 | 16.8 M/s | 71% | 255-context tree |
| lean replicated K=31 (theory only) | 28.2 M/s | 95% | reduced 8-probability model |
| **interleaved 4-engine** (shipped) | **20.7 M/s** | **19%** | 255-context tree |
| interleaved 8-engine @150 MHz | 22.3 M/s | 21% | 255-context tree |

Throughput figures are co-simulation, kernel-only. The point is the LUT column:
interleaving reaches the same throughput class as near-device-filling replication
while **keeping the full model**, so it gives up neither compression nor area.

## Negative results kept on purpose

- **The 400 MHz attempt** ([`REPRODUCIBILITY.md`](REPRODUCIBILITY.md) §8) — the one variant with an HLS Fmax above 400
  (`GROUPS=2` + registered DSP multiply, 402.58 MHz) **failed place & route** at the
  400 MHz clock. Design to the clock you get, not to Fmax.
- **Explicit 64-bit gather: 32.9% slower** (52,635 vs 39,623 cycles). The same
  change gave [`../tans`](../tans) a 5.4× speedup — wide AXI pays only when access
  is aligned and sequential. This design gathers an unaligned word at every chunk
  boundary.
- **DATAFLOW not pursued**: data movement is only ~15% of latency here, so perfect
  overlap could not beat ~1.18×.
