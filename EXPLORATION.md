# Exploration: workload classes → acceleration techniques → what actually happened

This is the project's core method, and it answers the spec's step 7b directly
("*explain differences between the calculated improvement and the actual
improvement*"). For each idea we state: the **workload property** that unlocks it
(and how we *measure* that property), the **technique** it enables, the
**theoretical** win, the **measured** result, and the **blocker** that stood in
between — because the blockers turned out to be the most transferable findings.

ARM Cortex-A53 baselines measured on the board: adaptive arith coder
**3.46 M sym/s**; static tANS **52.5 M sym/s** (it is a much cheaper algorithm).

---

## Part 1 — Workload classifiers (measured on our actual test data)

the workload profilers (branch `sadat-brainstorming`) contains the two profilers. The second is the important one:
for a bit-tree coder, what decides whether a bin is worth modelling is its
**conditional** entropy given its context, not its marginal entropy.

`H(bit | ctx)` per bit-tree level (MSB→LSB), and the resulting split:

| file | L0 | L1 | L2 | L3 | L4 | L5 | L6 | L7 | adaptive | bypass |
|---|---|---|---|---|---|---|---|---|---:|---:|
| image.pgm (photo) | .795 | .913 | .928 | .814 | .864 | .818 | .809 | .794 | **8** | 0 |
| img_smooth (gradient) | .542 | .892 | .732 | .911 | .931 | .936 | .938 | .999 | 7 | 1 |
| img_noise (random) | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | .999 | .998 | **0** | **8** |
| file0.bin (shared-table source) | .999 | **.216** | .637 | .888 | .969 | .992 | .996 | .997 | 3 | **5** |

Other classifiers (first profiler): order-0 entropy `H0`, max symbol probability
`p_max`, mean run length.

| file | H0 | p_max | mean run |
|---|---:|---:|---:|
| image.pgm | 6.734 | 0.112 | 1.63 |
| img_smooth | 6.882 | 0.034 | 2.83 |
| img_noise | 7.997 | 0.005 | 1.00 |
| file0.bin | 6.695 | 0.027 | 1.01 |

**Reading this table is the whole point:** the same coder has completely
different acceleration headroom on each file, and the classifier tells you which
technique applies *before* you build anything.

---

## Part 2 — The ideas, one by one

### Idea 1 — Independent blocks → replication / C-slow interleaving
**Classifier:** any data (blocks are made independent by construction).
**Theory:** K independent coders → K× throughput, since the interval recurrence
forbids intra-stream parallelism.
**Measured:** arith K=8 = **13.21 M/s** on fabric (3.8× ARM); K=16 cosim 16.8 M/s.
Interleaved (shared datapath) arith g4 = 20.7 M/s cosim at **18% LUT vs 71%**.
**Blocker → finding:** *the right parallelisation strategy depends on datapath
cost.* Interleaving wins when the per-lane datapath is expensive (arith: a
multiply/DSP per lane). For a **cheap** datapath (M-coder: table lookups) plain
replication wins — interleaving there only replicates context memory and couples
chunk-count to engine-count, hurting compression. Measured both ways.

### Idea 2 — Files share one frequency table (stationary) → static table coder ("tree method")
**Classifier:** the files are drawn from one distribution (verify: per-file
histograms match; `H0` stable across files).
**Theory:** precompute every interval bound once into a table, then code a whole
**byte per state transition** — no multiply, no per-symbol model update.
**Measured:** real tANS built and verified lossless. Software: **8.6× faster**
than the adaptive coder on CPU. HLS single stream: II=2, 251 MHz, **4% LUT, 0 DSP**.
Compression on genuinely shared-table data: **84.1%, i.e. it hits the source
entropy**; on non-stationary photos it loses 24–40% (static model can't track
local statistics).
**Blockers (a chain of four, each fixed — see Part 3):** HLS won't parallelise
unrolled calls; a shared ROM serialises lanes; then byte-serial AXI dominated.
**Final — BOARD VALIDATED:** SIMD + wide AXI, 4 files sharing one table:
**147.9 M sym/s on fabric vs 56.25 M/s ARM = 2.63×**, lossless, ratio 84.08%
(= the source entropy, i.e. the static table is optimal for this class).
Cosim said 194 M/s; the ~24 µs/call XRT overhead explains the gap — *calculated vs
actual differs at the host interface, not in the kernel* (spec step 7b).

> **Baseline honesty note.** Our first host used `tans.h`'s reference encoder as
> the ARM baseline and reported **8.1×**. That reference does a `vector::push_back`
> **per bit**; replacing it with the same efficient byte-packing encoder the kernel
> runs moved the ARM from 18.2 → 56.3 M/s and the speedup from 8.1× → **2.63×**.
> The honest number is 2.63×. A slow software baseline is the easiest way to
> manufacture a speedup, and we checked ours.

### Idea 3 — Multiply in the recurrence → replace with a table (adaptive)
**Classifier:** none (algorithmic).
**Theory:** the `range*prob` multiply is the critical path; a ROM read is shorter.
**Measured:** teammate's M-coder (H.264 CABAC engine) — 0 DSP, II=1, and it
*compresses better* than our range coder. Replicated K=8 = **31.1 M/s on fabric
(9.0× ARM)** — the project's best general-purpose design. Putting that core in our
interleaving harness: 22.6 M/s on fabric, 23% LUT (worse — see Idea 1's finding).

### Idea 4 — Near-uniform bins given context → BYPASS batching (**highest-value untried**)
**Classifier:** `H(bit|ctx) ≈ 1` for a bit-tree level (measured above).
**Theory:** a bin the model cannot beat costs exactly 1 bit. Code it in *bypass*:
no context read, no probability update, no adaptation — just shift the interval
and append the bit. Because bypass bins carry no model dependency, **k of them can
be coded in one cycle** (this is exactly what H.264/HEVC hardware does with
multi-bin bypass). Per-byte cost falls from 8 adaptive bins to
`n_adaptive + ceil(n_bypass / k)`.
**Predicted from our measured classifiers (k=4 bypass/cycle):**
| file | adaptive | bypass | cycles/byte now | with bypass batching | speedup |
|---|---:|---:|---:|---:|---:|
| img_noise | 0 | 8 | 8 | **2** | **4.0×** |
| file0.bin | 3 | 5 | 8 | **5** | 1.6× |
| img_smooth | 7 | 1 | 8 | 8 | 1.0× |
| image.pgm | 8 | 0 | 8 | 8 | 1.0× |
**Measured (software, all lossless):** applying each file's *measured* mask —
file0.bin 8→3 adaptive bins gives ratio **85.3% vs 85.8%**, and img_noise 8→0
gives **100.2% vs 101.7%**. Dropping bins the model cannot predict is **free, and
even slightly improves the ratio** (modelling an `H≈1` bin costs a little more
than the 1 bit a raw bit costs). For noise the full coder was actively harmful.
**Measured (HLS + cosim, 4095 B):**
| mask | adaptive bins | Fmax | cycles/byte | speedup |
|---|---:|---:|---:|---:|
| 0xFF | 8 | 192.8 MHz | 95.5 | 1.00× |
| 0x0E | 3 | 213.3 MHz | 47.3 | **2.02×** |
| 0x00 | 0 | 228.3 MHz | 6.9 | **13.9×** |
Cycles scale with the adaptive-bin count as predicted, **and Fmax rises** as
recurrence pressure drops (clock-adjusted: 2.24× and 16.4×). Full detail and the
caveat about absolute cycles/byte: `bypass_hybrid/RESULTS.md`.
**Honest note:** it accelerates *exactly* the files that compress worst
(incompressible ones). That is a real and useful trade — "we go fastest where
there is least to gain in ratio" — and it is the correct behaviour, not a flaw.

### Idea 5 — High skew / long runs → MPS run-mode
**Classifier:** `p_max` and mean run length (measured above).
**Theory:** when one symbol dominates, consecutive MPS bins barely move the
interval; detect a run and code it in one step (JPEG2000/JBIG2 MQ-coder has
exactly this "run-length mode"). Speedup ∝ mean run length.
**Measured classifier says: not applicable here.** Mean run lengths are 1.0–2.8
and `p_max ≤ 0.11`, so a run-mode would fire almost never. **Rejected on evidence
before implementation** — which is the profiler doing its job.

### Idea 6 — Near-max entropy → fixed-shift renormalisation
**Classifier:** `H0 ≈ 8`, and `H(bit|ctx) ≈ 1` everywhere (img_noise qualifies).
**Theory:** the data-dependent renorm loop is what blocks pipelining. If every bin
emits ~1 bit, the shift count is ~constant, so renorm becomes a fixed shift →
straight-line, fixed-latency body → II=1 at a higher clock.
**Status:** subsumed by Idea 4 (a fully-bypass coder *is* the fixed-shift coder,
and is strictly simpler). Kept here because the reasoning is the general one.

### Idea 7 — Energy, not just throughput (the spec asks for it)
**Theory:** a small kernel can win on **energy per byte** even without a large
throughput win, because it finishes sooner and/or draws less power.
**Measured on real hardware** — not estimated. The KV260 SOM has an INA260 sensor
(`/sys/class/hwmon/hwmon0/power1_input`); we sampled total board power under
12-second sustained loads of each engine:

| state | board power | throughput | **energy / byte** |
|---|---:|---:|---:|
| idle | 3.192 W | — | — |
| ARM software (tANS) | 3.349 W | 54.3 MB/s | **61.6 nJ/B** |
| FPGA kernel (tANS) | 3.422 W | 146.7 MB/s | **23.3 nJ/B** |

**The FPGA draws *more* instantaneous power (+0.073 W) yet uses 2.64× less energy
per byte**, because it finishes 2.70× sooner — classic race-to-idle. On an
incremental basis (subtracting the 3.192 W idle floor, i.e. the marginal cost of
doing the work) it is 2.89 → 1.57 nJ/B, a **1.84×** improvement.
**Takeaway:** energy tracks *throughput per watt*, and the accelerator wins on both
bases here. Reporting only instantaneous power would have made the FPGA look worse.

### Idea 8 — Use the rest of the chip: multiple compute units
**Classifier:** none — just look at the utilisation report. Our best kernel used
**16% LUT / 34% BRAM**, i.e. two thirds of the fabric was idle while we spent days
tuning the one kernel inside it.
**Theory:** blocks are independent, so N copies of the kernel should give N×,
provided nothing shared (DDR bandwidth, XRT dispatch) saturates first.
**Built:** `nk=arith_kernel:2` with each CU on its **own HP port pair**
(CU1→HP0/HP1, CU2→HP2/HP3) so they do not contend for AXI. Both CUs live in one
bitstream, so running 1 vs 2 isolates the effect exactly.
**Measured on fabric:**
| | throughput | vs ARM (56.25 M/s) |
|---|---:|---:|
| 1 CU | 146.75 M sym/s | 2.61× |
| **2 CUs** | **286.65 M sym/s** | **5.10×** |
→ **1.95× scaling (2.00× = perfect)**, both lossless. Neither DDR bandwidth nor
dispatch was a limit at this rate. **This was the single largest win available**,
and it needed one line of `link.cfg` — a humbling contrast with the weeks of
micro-optimisation inside the kernel. BRAM (34%/CU, from the per-lane table
copies) caps it at 2 CUs here; a smaller table would allow more.

### Idea 9 — Model the right thing: spatial / higher-order context
**Classifier:** conditional entropy under candidate contexts (the context study (branch `sadat-brainstorming`)).
**Theory:** we modelled each byte in isolation (order-0). Images are spatially
redundant, so conditioning on neighbouring pixels should compress far better.
**Measured:** MED predictor (JPEG-LS style) reaches **1.202 bits/sym = 15.0%** on
image.pgm, against our order-0 **6.734 b/sym = 84.2%**.
**The hardware catch, and it is the interesting part:** our bit-tree needs 255
probability states *per context*. Order-0 = 1 context = **0.4 KB**, which is why
16 lanes fit and why all our parallelism was affordable. A MED model needs ~242
contexts ≈ **91 KB per lane** → only ~2-3 lanes fit instead of 16.
**Conclusion: compression and parallelism compete for the same BRAM.** The ~4×
ratio win would cost ~5-8× of our lane count. Software-measured only; not built.

---

## Part 2b — Robustness: does any of this work on real files?

Tested rather than assumed (`ARBITRARY_FILES.md`): a 1.4 MB PDF, an aarch64 ELF
binary, markdown, and images, encoded then decoded and compared byte-for-byte.
- **Adaptive coders: fully general** — lossless on everything, ratios behaving
  sensibly (already-compressed PDF 98.4%, structured binary 28.7%).
- **Tree method: always correct, but only compresses its own class** — lossless on
  everything, yet *expands* off-class data (ELF binary 143.3%).
- A static-table coder must **smooth** its table (every symbol ≥ 1 slot) or it is
  not total: our first table had 16 zero-probability symbols and **segfaulted** on
  out-of-alphabet input. Smoothing costs 0.088% ratio and makes an unseen byte cost
  12 bits instead of being undefined.

---

## Part 3 — The blocker taxonomy (the most transferable result)

Every one of these cost us real time and each has a general lesson.

| # | Blocker | Symptom | Fix | Measured effect |
|---|---|---|---|---|
| B1 | Loop-carried recurrence, distance 1 | won't pipeline / II≫1 | C-slow interleave K states through one datapath | II=1 achieved; arith Fmax 293 |
| B2 | Variable-trip renorm loop inside the coding loop | pipelining never converges (>14 min) | closed-form / flat state machine | enabled II=1 |
| B3 | Multiply inside the recurrence | 2.42 ns of a 5 ns budget; capped at 160–200 MHz | table lookup (M-coder), or register the DSP | g8 160→192 MHz; g2 → 402 MHz |
| B4 | **HLS does not parallelise unrolled calls** | K lanes ran *sequentially* (199,340 cyc for K=8 = 8× one lane) | rewrite as a **SIMD lockstep loop** (one iteration advances all K lanes) | serial → parallel |
| B5 | **Shared ROM = one read port** | K lanes cannot look up in the same cycle | **replicate the table per lane** | required for B4 to pay off |
| B6 | **Byte-serial `m_axi`** | encode was only ~9% of runtime; I/O 90% | **64-bit aligned wide AXI** | **91,582 → 16,892 cycles (5.4×)** |
| B6a | *(B6 refinement)* the value of fixing I/O depends on **compute intensity per byte** | tANS: 2 cyc/byte → I/O = 90% of runtime. M-coder: ~7 cyc/byte (8 bins) → I/O = only 21% | widen I/O for cheap coders; for expensive ones fix the datapath first | wide AXI = **5.4× on tANS**, but only **~1.2× on the M-coder** — measured from its loop breakdown, so we did *not* spend a build on it |
| B7 | Platform offers only fixed 100/200/400 MHz | a 293 MHz design still runs at 200 | design *to the clock*, not to Fmax | arith interleaved lost its entire advantage |
| B7a | **HLS Fmax is an estimate, not a guarantee** | the one design with Fmax > 400 (arith g2 + reg-mul, **402.58 MHz**) **failed place & route at 400 MHz**: `[VPL 101-2] design did not meet timing - pulse width violation` | treat a <1% margin over a clock step as no margin; HLS models logic delay, not routing/skew/pulse-width | **400 MHz attempt failed** — see `interleaved/board/400MHZ_RESULT.md` |
| B8 | XRT per-call overhead ≈ 60 µs | small blocks are latency-bound | batch large blocks | 256 B = 3.8 M/s vs 4 KB = 22.6 M/s |
| B9 | Unaligned chunk boundaries defeat wide I/O | earlier wide-input attempt was **33% slower** | power-of-two chunking → every chunk 8-byte aligned | made B6's fix possible |
| B10 | Idealised co-simulation AXI | cosim said 31 M/s, fabric gave 11.7 | trust fabric; check burst inference | the arith interleaved kernel lost ~1.46× to real DDR |

**B4+B5+B6 together are the story of the tree method**: the same algorithm went
**28.2 → 35.8 → 194 M sym/s** in cosim purely by removing hardware blockers, with
no change to the compression algorithm at all.

---

## Part 4 — Scoreboard

| design | workload class it targets | throughput | where measured | area |
|---|---|---:|---|---:|
| naive arith (single stream) | — | 0.29–2.4 M/s | fabric | — |
| arith K=8 replicated | any | 13.2 M/s | **fabric** | 37% LUT |
| arith interleaved g4 | any (area-limited) | 11.7 M/s | **fabric** | 18% LUT |
| M-coder replicated K=8 | any | **31.1 M/s** | **fabric** | ~35% LUT |
| M-coder interleaved g4 | any (area-limited) | 22.6 M/s | **fabric** | 23% LUT |
| **tANS SIMD + wide AXI** | **shared frequency table** | **147.9 M/s (2.63× ARM)** | **fabric** | 16% LUT |
| **tANS × 2 compute units** | **shared frequency table** | **286.6 M/s (5.10× ARM)** | **fabric** | 32% LUT, 68% BRAM |
| bypass-hybrid, mask 0x0E | partially-modelable data | 2.02× fewer cycles | cosim | — |
| bypass-hybrid, mask 0x00 | **incompressible data** | **13.9× fewer cycles** | cosim | — |

Note the two coders have *different ARM baselines* because they are different
algorithms: the adaptive arith coder runs at 3.46 M sym/s on the A53, the static
tANS at 56.3 M sym/s. So "13.2 M/s at 3.8×" and "147.9 M/s at 2.63×" are both
true and not comparable as raw numbers — another reason the workload class, not a
single speedup, is the right unit of comparison.

The headline is not one number. It is that **the best design depends on the
workload class**, and we can *measure* which class a file is in before choosing.
