# Final Presentation — 10 slides
**Hardware Accelerated Compression using Arithmetic Coding**
Rory Self · Bryan Bong · Ujjwal Uberoi · Sadat Kabir — 20 min, 11:30 am 3 Aug

> Speaking roles marked **[R] [B] [U] [S]**. Numbers below are all measured; fabric
> results say "board", the rest are cycle-accurate co-simulation.

---

## Slide 1 — Project overview **[R]**
**Accelerating lossless compression (adaptive arithmetic coding) on the Kria KV260.**

- Arithmetic coding represents a whole message as one number in [0,1): each symbol
  shrinks the interval by its probability. Adaptive = the model learns as it reads.
- Chosen because it is **compute-bound, sequential, and widely used** (it is the
  entropy stage inside JPEG2000, H.264/CABAC, Zstandard).
- Deliverables: a software reference on the ARM, four accelerated HLS designs on
  the fabric, and a measured comparison of both.

*Speaker note: state the KV260 has an ARM Cortex-A53 (1.33 GHz) PS and programmable logic at a fixed 100/200/400 MHz.*

---

## Slide 2 — Aims & the central obstacle **[R]**
**Aim:** find where FPGA acceleration genuinely helps this algorithm — and where it does not.

**The obstacle that shapes everything:** encoding symbol *N* needs the interval
produced by symbol *N−1*. A **sequential recurrence** with no way around it.
- You cannot parallelise a single stream.
- You cannot cleanly pipeline it either — renormalisation emits a *variable*
  number of bits per symbol.
- **Measured:** a naive HLS port runs at **0.29–2.4 M sym/s vs the CPU's 3.46** —
  i.e. the FPGA is *slower*. Same sequential work at 200 MHz instead of 1.33 GHz.

**So the project became: what has to change for the FPGA to win?**

---

## Slide 3 — Solution approach & our use of AI **[B]**
**Approach: measure a workload classifier first, then pick the technique.**
We stopped asking "how fast can we make *the* coder" and started asking *"which
workload class unlocks which technique"* — then built only what the measurement justified.

**Use of AI (Claude Code) — what it did well:**
- Rapid exploration: generated and swept HLS variants (K-way, C-slow interleaving,
  SIMD, wide AXI) far faster than by hand; ran the synthesis/co-sim loop.
- Diagnosis from tool output: read `csynth` critical-path reports and identified the
  interval multiply, the shared-ROM read port, and byte-serial AXI as the blockers.

**Where AI was wrong — and how we caught it (all verified against measurement):**
| AI claim | Reality | How caught |
|---|---|---|
| tANS is "8.1× faster on board" | **2.63×** | Its ARM baseline used a reference encoder doing a `vector::push_back` per *bit*; the fair baseline is 3× faster |
| Interleaving would beat replication for the M-coder | It **lost** (22.6 vs 31.1 M/s) | Built both, measured on fabric |
| DATAFLOW worth ~1.27× | ~1.03× | Re-read the code: the encode needs 75% of the input before it starts |
| HLS Fmax 402 MHz ⇒ use the 400 MHz clock | **Failed place & route** | Ran the build |

**Lesson: AI compressed the search, but every claim needed a measurement. The
speedup we nearly reported was 3× too high.**

---

## Slide 4 — HW/SW architecture **[B]**
```
 ARM Cortex-A53 (PS)                         Programmable Logic (PL @200 MHz)
 ┌──────────────────────┐   AXI-Lite ctrl   ┌───────────────────────────────┐
 │ XRT host             │──────────────────►│ arith_kernel                   │
 │  • splits into blocks│                   │   Load  (m_axi, 64-bit burst)  │
 │  • enqueues kernels  │   m_axi (HP0-3)   │   ┌─────────────────────────┐  │
 │  • decodes output    │◄─────────────────►│   │ K parallel coder lanes  │  │
 │    to VERIFY lossless│    DDR            │   └─────────────────────────┘  │
 │  • times with chrono │                   │   Store (header + chunks)      │
 └──────────────────────┘                   └───────────────────────────────┘
```
- **Partition:** the PS does file I/O, blocking and verification; the PL does only
  the compute-bound coding loop.
- **Parallelism has two levels:** *within* a kernel (K independent coder lanes) and
  *across* kernels (multiple compute units on separate HP ports).
- **Verification is in the loop:** the host decodes the board's own output every
  run, so every performance number shown is also a correctness check.

---

## Slide 5 — Method: how we measured **[U]**
- **Software baseline** on the board's own A53 (not a laptop), `-O3`, timed with
  `std::chrono` around **only** the encode call.
- **Hardware** timed around **only** kernel enqueue + wait — no file I/O, no setup.
- **Correctness** every run: host decodes the board's compressed bytes and compares
  byte-for-byte with the input.
- **Energy** measured on the board's **INA260 power sensor** under 12-second
  sustained load — not estimated from a tool report.
- **Co-simulation vs fabric reported separately**, because they disagree (Slide 7).

*Workload classifiers we measure before choosing a technique:* order-0 entropy,
per-bit-tree-level conditional entropy `H(bit|ctx)`, symbol skew, run length.

---

## Slide 6 — Results **[U]**
**On the board (4 KB blocks unless noted):**

| design | technique | throughput | vs its ARM baseline |
|---|---|---:|---:|
| naive HLS | none | 0.29–2.4 M sym/s | **0.08–0.69× (slower!)** |
| K=8 replicated | replication | 13.2 M sym/s | 3.8× |
| interleaved arith | C-slow, shared datapath | 11.7 M sym/s | 3.4× |
| **M-coder (CABAC)** | **multiply-free tables** | **31.1 M sym/s** | **9.0×** |
| **tree method (tANS)** | **static table, byte-wise** | **147.9 M sym/s** | 2.63× |
| **tANS × 2 CUs** | **+ multi-instance** | **286.6 M sym/s** | **5.10×** |

**Energy:** FPGA **23.3 nJ/byte** vs ARM **61.6 nJ/byte** = **2.64× less energy**
(it draws +0.07 W more but finishes 2.7× sooner — race to idle).

⚠️ **Ratios are not comparable across rows** — each is against its own software
baseline, and those differ ~20× (bit-wise 2.76 vs byte-wise 56.3 M sym/s).
**Compare the throughput column.**

---

## Slide 7 — What worked, and the biggest surprise **[S]**
**Worked — in order of payoff:**
1. **Multi-instance (one line of `link.cfg`): 1.95× scaling.** Our kernel used 16%
   of the LUTs — two-thirds of the chip sat idle while we tuned the kernel inside it.
2. **Removing the multiply** (table-driven CABAC): 0 DSP, and *better* compression.
3. **Wide aligned AXI: 5.4×** on tANS — I/O was 90% of its runtime.
4. **C-slow interleaving** to break the distance-1 recurrence: II=1 at 18% LUT vs 71%.

**The surprise: the design that won in simulation lost on hardware.**
Interleaved arith: **31 M/s in co-sim → 11.7 M/s on fabric**. Two causes, both
measurable: the platform's clocks are quantised (its 293 MHz Fmax still runs at
200), and its `m_axi` does not burst, costing ~1.46× on real DDR that co-simulation
hid behind an idealised memory model.

---

## Slide 8 — What didn't work, and why **[S]**
| Attempt | Outcome | Why |
|---|---|---|
| Naive single-stream port | 15× **slower** than CPU | The recurrence — no parallelism to exploit |
| Pipelining the renorm loop | Never converged (>14 min) | Variable-rate output can't fit a fixed-II pipeline |
| Interleaving the **M-coder** | 22.6 vs 31.1 M/s — **lost** | Interleaving pays only when the *datapath* is expensive; the M-coder's is cheap tables, so plain replication wins |
| Lean K=31 (95% LUT) | 28.2 M/s but **won't route** | Over the practical placement ceiling |
| **400 MHz clock** | **Failed place & route** | HLS Fmax 402 vs a 400 target = 0.6% margin; the estimate ignores routing, skew, pulse width |
| Static tANS on general data | Expands an ELF binary to 143% | A static table only fits its own workload class |
| Wide AXI on the M-coder | Not built | Measured its I/O at 21% ⇒ ceiling ~1.2×; not worth a build |

**Negative results were cheap because we measured the classifier first** — e.g. we
rejected an MPS run-mode outright after measuring mean run length at 1.0–2.8.

---

## Slide 9 — Given more time **[S]**
1. **Model the right thing.** Everything we built is **order-0** (a byte in
   isolation). A MED/JPEG-LS **spatial** predictor reaches **15.0%** on our test
   image vs our **84.2%** — a ~4× compression win we never took.
   *The catch we quantified:* it needs **91 KB of context memory per lane** vs
   0.4 KB, cutting 16 lanes to ~2. **Compression and parallelism compete for the
   same BRAM** — arguably the project's most interesting trade.
2. **The decoder** — we accelerated only the encoder; decode matters more in
   read-many systems and has a harder recurrence.
3. **A fair multi-core baseline** — our ARM figures are single-threaded on a
   4-core A53; a threaded baseline would cut the ratios and is the honest comparison.
4. **More compute units** — the M-coder fits 3; BRAM is the limit, so a smaller
   context memory buys more parallelism.

---

## Slide 10 — Conclusion **[R]**
**There is no single speedup number for this algorithm — the achievable
acceleration is a function of the workload class, and the class is measurable up
front.**

- Sequential-recurrence algorithms need parallelism *found*, not *extracted*:
  replication, interleaving, or more compute units.
- **The best technique depends on the datapath cost** (interleave when expensive,
  replicate when cheap) and on the **workload** (a static table wins only when files
  share a distribution).
- **Simulation ≠ silicon:** clock quantisation and un-burstable I/O reversed our
  co-simulation ranking. We report the fabric numbers.
- Best general-purpose result **9.0× (M-coder)**; best specialised **286.6 M sym/s**
  at **2.64× less energy**.
- **Biggest lesson:** before optimising the kernel, check whether you are using the
  whole chip. One line of configuration beat weeks of micro-optimisation.
