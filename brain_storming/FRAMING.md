# Framing the work: acceleration is a *regime*, not a number

> **Thesis.** There is no single speedup figure for adaptive arithmetic coding on
> an FPGA. The achievable acceleration is a function of the operating regime.
> Our contribution is that we *mapped that regime* — across data, architecture,
> platform, block size, and model — instead of quoting one headline number.

All figures below are measured (real board via XRT, or cycle-accurate C/RTL
co-simulation, as noted). ARM baseline = Cortex-A53 @ 1.33 GHz, **3.46 M sym/s**,
measured on the board.

---

## Axis 1 — Input statistics (data dependence)

Same kernel (K=8), same board, two 256×256 images:

| workload | compressed size | ARM | FPGA | speedup |
|---|---|---:|---:|---:|
| smooth / compressible | 46,157 B (70.4%) | 2.92 MB/s | 10.34 MB/s | **3.54×** |
| random / incompressible | 66,916 B (102.1%) | 2.46 MB/s | 9.43 MB/s | **3.83×** |

Two distinct effects, both real:
- **Absolute throughput tracks compressibility** — compressible data emits fewer
  renormalisation bits per symbol, so *both* CPU and FPGA run faster on it.
- **The speedup *ratio* moves the opposite way** — incompressible data does more
  real coding work per symbol, so the fixed per-block overhead amortises better,
  giving a slightly *higher* ratio (Amdahl).

**Punchline:** the workload that compresses best and the one that accelerates
best are not the same workload. Any speedup claim must state the input.

---

## Axis 2 — How you spend the silicon (architecture)

The interval-update recurrence is inherently sequential, so a single stream is
*slower* than the CPU. Parallelism must come from one of two strategies:

| strategy | idea | area | best result |
|---|---|---|---|
| **Replication** (iter 1) | K independent coders side by side | high | K=8 = 3.8× ARM (fabric); K=31 = 8.2× ARM but **95% LUT**, reduced model |
| **C-slow interleaving** (iter 2) | one shared pipeline, LANES states round-robin → II=1 | low | g4 = 9.0× ARM @ **18% LUT**, full 255-ctx model (cosim) |

Interleaving is the stronger *architectural* result: same throughput class at a
fifth of the area, with the full-quality model. Enabling moves: exact-width
`ap_uint` datapath + a **registered DSP multiply** (the multiply in the
recurrence was the measured critical path; registering it took g2 to 402 MHz).

---

## Axis 3 — Simulation vs silicon (the finding that ties it together)

**The design that won in simulation lost on hardware.**

| design | cosim | on fabric (200 MHz, 4 KB) |
|---|---:|---:|
| interleaved g4 | **31 M/s** (9.0× ARM, 293 MHz, 18% LUT) | 11.7 M/s (3.4×) |
| K=8 replication | 12.9 M/s | **13.2 M/s** (3.8×) |

The interleaved kernel is lossless and correct on fabric (compresses 4095→1904 B,
byte-identical to cosim) — but slower than replication, for two reasons cosim hid:
1. **Clock cap.** The `kv260_custom` platform only offers fixed 100/200/400 MHz
   clocks. g4's advantage needed ~293 MHz; it is forced to 200, discarding the
   headroom that made it win.
2. **Idealised AXI.** Cosim models memory as a flat 64-cycle latency. The
   interleaved kernel's I/O does not burst ("inferred burst reverted"), so on real
   DDR its Load/Store add ~1.46× to the kernel time. Replication bursts cleanly
   and matches its cosim.

**Punchline:** "fastest in simulation" and "fastest on this board" are different
scenarios. Reporting only cosim would have been wrong.

---

## Axis 4 — Block size (overhead amortisation)

Fixed per-call cost ≈ 62 µs (XRT enqueue + wait), so throughput is a function of
how much you feed per call (interleaved g4, on fabric):

| message | 256 B | 1 KB | 2 KB | 4 KB | asymptote |
|---|---:|---:|---:|---:|---:|
| throughput | 3.2 M/s | 7.3 | 9.6 | 11.7 | ~14.2 |

Small-message (latency-bound) and bulk (compute-bound) are different operating
points. Fit: `time = overhead + N × per-symbol` → g4: 62 µs + 70.5 ns/sym;
K=8: 55 µs + 62.2 ns/sym.

---

## Axis 5 — Model: quality vs speed

| model | compression | speed | needs |
|---|---|---|---|
| adaptive (current) | best on non-stationary data | has the multiply recurrence | nothing |
| static table-driven (**tANS/FSE**) | ~same at order-0 for stationary data | no multiply, ~1 op/byte | a fixed frequency table |

A knob that trades adaptivity for raw speed — the basis of iteration 3 below.

---

## Headline conclusions (state these plainly)

1. Single-stream arithmetic coding **cannot** beat the CPU; acceleration is
   fundamentally about extracting parallelism from a sequential recurrence.
2. The best architecture is **regime-dependent**: replication wins on *this*
   fixed-clock board; interleaving wins on *area-efficiency* and would win on
   throughput given a ~300 MHz clock.
3. Speedup is **data-dependent** in two separate ways (absolute throughput vs
   ratio) — a single number is meaningless without the workload.
4. **Simulation ≠ silicon**: platform clock quantisation and un-burstable I/O
   reversed the ranking. Negative results, reported as findings.

---

## Suggested figures for the report

1. **Speedup-vs-workload** bar pair (Axis 1): compressible vs incompressible,
   showing throughput bars *and* the ratio line crossing over.
2. **Throughput-vs-area scatter** (Axis 2/3): each design a point
   (x = LUT %, y = M sym/s), cosim vs fabric markers — visually shows interleaving
   dominating area and the cosim→fabric drop.
3. **Throughput-vs-block-size curve** (Axis 4): the amortisation curve toward the
   asymptote, annotated with the 62 µs overhead.
4. **Critical-path / Fmax bar** (Axis 2): g8/g4/g2 Fmax before/after the
   registered multiply, against the 200/400 MHz platform lines.

---

## Next: Iteration 3 — static, table-driven coder (tANS/FSE)

**Goal:** attack the two things that actually limited fabric throughput —
the multiply in the recurrence *and* the un-burstable I/O — at once.

- **Replace arithmetic with a table lookup.** Build the entropy table once for a
  fixed frequency histogram, then encode a whole *byte* per state transition
  (`state = table[state][symbol]`), no multiply. This is the entropy stage inside
  Zstandard / LZFSE. It (a) removes the 2.4 ns multiply that was our critical
  path → high Fmax even at high parallelism, and (b) cuts ~9 ops/byte to ~1.
- **Requirement / trade-off:** the model becomes *static* — needs one frequency
  table (transmit ~a few hundred bytes, or use a fixed table). For the
  "same-symbols / same-frequency-table" small-image scenario this costs almost no
  compression (both are order-0), which we will confirm in software first.
- **Pair with bursted I/O.** Redesign Load/Store for coalesced/wide `m_axi`
  (dataflow load → compute → store) so real DDR stops being the tax that cosim
  hid. This is the *other* half of the Axis-3 lesson.

**Plan of record:**
1. Prototype static byte-wise tANS in software; measure compression vs the
   current adaptive coder on a real image (confirm the loss is small).
2. HLS: table-lookup coder, measure Fmax + cycles/byte vs iteration 2.
3. Add bursted/coalesced I/O; re-measure on fabric.
4. Fold results in as a sixth data point on the throughput-vs-area chart.

**Expected story:** iteration 3 is the design that finally makes the *fabric*
number match the *simulation* number — because it removes both hidden taxes.
