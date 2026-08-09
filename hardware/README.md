# hardware — FPGA implementations (Kria KV260)

Four HLS designs for the same problem: accelerating adaptive arithmetic coding.
All were built, linked and **measured on the board**, and every run verifies its own
output by decoding it back and comparing with the input.

All figures below are **kernel-only** (see the boundary rule further down) and were
re-measured on 2026-08-08.

| design | idea | on fabric | vs its ARM baseline |
|---|---|---:|---:|
| [`replication_full`](replication_full) | K independent coders side by side | 13.27 M sym/s | 3.82× |
| [`mcoder`](mcoder) | same K-way structure, but the `range × prob` multiply becomes a 384-byte ROM lookup → **0 DSP**, II=1 | **33.07 M sym/s** | **9.53×** (2.49× `replication_full`) |
| [`interleaved`](interleaved) | one shared datapath, 16 coder states C-slow interleaved → II=1 at 19% LUT | 11.67 M sym/s | 3.36× |
| [`tans`](tans) | static table, one lookup per **byte**; ×2 compute units | 147.9 → **286.6** M sym/s | 2.63× / 5.10× |

## Reproducing any of these

**Every number we publish has a `REPRODUCIBILITY.md` next to the design that
produced it**, naming the file to use, the command to run, where to run it
(workstation or board), and the output to expect:

- [`replication_full/REPRODUCIBILITY.md`](replication_full/REPRODUCIBILITY.md)
- [`mcoder/REPRODUCIBILITY.md`](mcoder/REPRODUCIBILITY.md)
- [`interleaved/REPRODUCIBILITY.md`](interleaved/REPRODUCIBILITY.md)
- [`tans/REPRODUCIBILITY.md`](tans/REPRODUCIBILITY.md)

Raw logs live in each design's `results/`. Two rules apply across all four:

### Rule 1 — never mix the two timing boundaries

| boundary | host | measures |
|---|---|---|
| **kernel-only** | `bench_host` | `chrono` around `enqueue+wait`. Excludes host↔device transfer |
| **image demo** | `demo_host` | the whole compress call. **Includes** transfer |

The same M-coder bitstream reads as **33.07 M sym/s** kernel-only and **22.25 M
sym/s** in the image demo. Both are correct; putting them in one table is not. This
is exactly the error on the presentation's results chart, where three kernel-only
bars sit beside one image-demo bar.

### Rule 2 — compare throughput, not ratios, and never compare ratios across inputs

Each speedup is against that design's *own* software baseline, and those differ
~16× (the bit-wise arith coder runs at 3.47 M sym/s on the A53; the byte-wise tANS
at 56.25). A ratio is meaningless without its baseline.

Compression **ratios** are comparable only on byte-identical input. The four
designs are currently measured on three different workloads, and input choice alone
moves throughput ~14% and ratio from 65% to 102% (see
[`replication_full/results/image_demo.txt`](replication_full/results/image_demo.txt)).
`tans` cannot use the others' input without defeating its premise — its table is
baked from its own distribution.

## The finding that cuts across all four: it stopped being about the coder

Every design ended up limited by **moving bytes**, not by coding them — and the
share differs enormously, which is why the same optimisation helps one design and
hurts another. All measured, all reproducible from the files above.

| design | data movement is… | evidence |
|---|---|---|
| [`replication_full`](replication_full) | small enough to ignore; bursts infer cleanly | cosim 12.9 vs fabric 13.27 M/s — **agree to 3%** |
| [`interleaved`](interleaved) | ~15% of latency, but bursts **revert** → **1.46×** DDR penalty | cosim 198 µs → 289 µs real |
| [`mcoder`](mcoder) | **84% of runtime** — the coder is only 15.6% | 4,096 of 26,317 cycles |
| [`tans`](tans) | **90% of runtime**, until the port was widened | 35.8 → **194 M/s** |

### Throughput per LUT — why byte-wise wins on area too

The M-coder codes 8 bins per byte (II=1 → 8 cyc/byte); tANS codes one byte per state
transition (II=2 → 2 cyc/byte). So even a *single* tANS lane does fewer cycles per
byte than an 8-way M-coder, at a fraction of the area:

| design | throughput | LUT | DSP | M/s per %LUT |
|---|---:|---:|---:|---:|
| M-coder replicated K=8 (fabric) | 33.1 M/s | ~36% | 0 | 0.9 |
| interleaved g4 (fabric) | 11.7 M/s | 19% | 6 | 0.6 |
| **tANS single lane (cosim, kernel)** | **48.6 M/s** | **4%** | **0** | **~12** |

~14× the throughput per LUT. The catch is the model — tANS is static, so this only
holds on data matching its baked table. There is no free lunch here, only a
different trade.

### Energy per byte — measured on the board, all designs

Not from a synthesis report: Vitis reports resources and timing only, and Vivado's
`report_power` is a switching-model estimate. These come from the KV260 SOM's
**INA260 sensor** (`/sys/class/hwmon/hwmon0/power1_input`, total board power in µW)
sampled ~5×/second while each workload runs flat out.
`energy per byte = board power ÷ throughput`.

| workload | throughput | board power | **energy / byte** |
|---|---:|---:|---:|
| ARM software — arith coder | 3.46 M sym/s | ~3.36 W\* | **~971 nJ/B** |
| FPGA — interleaved | 11.61 M sym/s | 3.362 W | **290 nJ/B** |
| FPGA — replication K=8 | 13.23 M sym/s | 3.318 W | **251 nJ/B** |
| ARM software — tANS | 54.35 M sym/s | 3.361 W | **61.8 nJ/B** |
| FPGA — tANS, 1 CU | 145.7 M sym/s | 3.443 W | **23.6 nJ/B** |
| **FPGA — tANS, 2 CUs** | **286.6 M sym/s** | 3.634 W | **12.7 nJ/B** |

\* the arith software baseline's power was not measured directly; a saturated A53
core draws ≈3.36 W total (measured for the tANS software run), so this row is an
estimate. Every other row is measured. Idle floor: **3.22 – 3.29 W** per session.

Four things this says:

1. **Energy per byte is set by throughput, not power.** Board power varies only
   3.32 → 3.63 W across everything (~9%) while throughput varies 3.5 → 287 M sym/s
   (82×). The fastest design is essentially always the most energy-efficient —
   *race to idle* dominates.
2. **Multi-instance is also the best energy result**: two compute units nearly halve
   energy per byte (23.6 → 12.7 nJ/B) for ~5% more power. Parallelism at constant
   clock is close to free, energetically.
3. **Interleaving costs energy versus replication** (290 vs 251 nJ/B) — slightly
   slower *and* slightly more power. It trades DSPs for BRAM (50 vs 26 blocks), and
   BRAM accessed every cycle is not cheap. The area win does not become an energy win.
4. **Hardware beats software in every pair**, but by very different margins: ~3.9×
   for the arith coder (971 → 251) and 2.6× for tANS (61.8 → 23.6), rising to 4.9×
   with two compute units.

> **Caveat.** The idle floor drifts ~70 mW between sessions, and the smallest
> incremental signal here (replication, ~32 mW above idle) is close to that. The
> **total-power** figures are robust; incremental-power figures are indicative only.

#### Reproducing the energy rows

🔌 board. Run one engine flat out and sample the sensor during the run — the two
design families need different hosts:

```sh
# tANS rows: demo_host has sustained-load modes
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -m sw -t 14'   # software only
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -m hw -t 14'   # hardware only

# arith rows (replication_full, interleaved): loop bench_host long enough to sample
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./bench_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 40000'

# in a second shell, sample throughout the run (microwatts, ~5 Hz):
ssh "$BOARD" 'while :; do cat /sys/class/hwmon/hwmon0/power1_input; sleep 0.2; done'
```

Take the idle reading first, with nothing running — it drifts between sessions and
the caveat above depends on it. Sensor identity:
`cat /sys/class/hwmon/hwmon0/name` → `ina260_u14`.

Raw log for the tANS rows:
[`tans/results/energy_measurement.txt`](tans/results/energy_measurement.txt); the
method is written up in [`tans/REPRODUCIBILITY.md`](tans/REPRODUCIBILITY.md) §6.
The arith rows were measured the same way with the `bench_host_arm` command above.

### Per-call overhead (XRT), measured as the intercept of an N-sweep

| design | fixed cost/call | per-symbol | block size |
|---|---:|---:|---|
| `replication_full` | ~55 µs | 62.2 ns | 4 KB |
| `interleaved` | ~62 µs | 70.5 ns | 4 KB |
| `tans` | ~24 µs | — | 16 KB |

At N=4095 the fixed part is ~18% of a call; at N=256 it is ~77%. **This is why every
throughput figure in this repo is quoted at the largest block** — a small-block
number measures XRT, not the design.

### Wide AXI and streaming: measured, with opposite results

| change | `tans` | `interleaved` |
|---|---:|---:|
| 64-bit AXI port | **+5.4×** (35.8 → 194 M/s) | **−32.9%** (39,623 → 52,635 cyc) |
| DATAFLOW / streaming | **−50%** (16,851 → 25,461 cyc) | ≤1.18× possible, not built |

Two conclusions we can actually defend:

- **Wide AXI is not a general win.** It pays when reads are aligned and sequential
  (tANS), and costs when the design must gather an unaligned word at every chunk
  boundary (interleaved, and by extension mcoder). *Fix the access pattern before
  widening the port.*
- **Streaming is blocked by the algorithm, not the tooling.** tANS encodes in
  **reverse**, so the coder cannot start until the whole block has arrived — Load
  can never overlap the encoder. On `interleaved` the ceiling is a mere 1.18×
  because arithmetic, not I/O, dominates there. Neither is a tuning failure.

The unfinished work is clear from this table: **`mcoder` is 84% data movement and
never got a wide port.** It is the single biggest remaining win, and it would need a
striped input layout first.

Every design has the same layout:
```
src/     the coder, the board top (arith_kernel), and the hosts
           bench_host  HARDWARE ONLY  -- profiles the kernel, verifies lossless
           demo_host   SW vs HW       -- times both, what the demo runs
           multi_host  CU SCALING     -- 1 CU vs 2 CUs (tans only)
test/    testbenches (C simulation + co-simulation)
synth/   hls.cfg (C-synthesis/co-sim), board.cfg (-> .xo), link.cfg (-> .xclbin)
bin/     prebuilt bitstream + cross-compiled aarch64 hosts, so the demo runs
         without a 10-40 min Vivado rebuild
data/    inputs
results/ the measurements
```

## Run any of them
```sh
source ../env.sh              # Vitis, platform, sysroot, ARM cross-compiler
../run_on_board.sh            # list designs
../run_on_board.sh tans       # deploy, load the PL, run, verify
../run_on_board.sh all        # all of them + a comparison table
```
Each design's `README.md` has the full path from source to bitstream to board.

**The board has no compiler** (no g++, no XRT headers), so every host is
cross-compiled on the workstation — that is what `env.sh` sets up.
