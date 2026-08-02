# C-slow interleaving, explained for the slide

## 1. The problem in one line
Encoding symbol *N* needs the interval produced by symbol *N−1*. That chain —
`read prob → multiply → add → store` — takes about **6 ns of logic**, and with a
plain loop it must **all fit inside one clock cycle**.

```
    one stream, dependence distance = 1
    ┌─────────────────────────────────────────┐
    │  sym0 ─► [read▸mul▸add▸store] ─┐         │   the next symbol cannot start
    │                                 ▼         │   until this finishes
    │  sym1 ─────────────────► [read▸mul▸add▸store]
    └─────────────────────────────────────────┘
       6 ns of logic must fit in ONE cycle  ⇒  clock ≤ ~160 MHz
```

The scheduling rule HLS applies (standard modulo scheduling):

> **II ≥ ⌈ recurrence delay ÷ dependence distance ⌉**

With distance = 1 you are stuck: either a slow clock, or II > 1 (a stall every cycle).

## 2. The trick: don't wait — work on someone else
Split the input into **LANES independent streams**. They share nothing, so while
lane A's update is still in flight, the datapath can start lane B, then C, then D.
By the time we come back to A, A's result has landed.

```
    one shared datapath, 4 lanes interleaved  (dependence distance = 4)

    cycle:   1     2     3     4     5     6     7     8
           ┌────┬────┬────┬────┬────┬────┬────┬────┐
    lane:  │ A0 │ B0 │ C0 │ D0 │ A1 │ B1 │ C1 │ D1 │   ← one symbol issued EVERY cycle
           └────┴────┴────┴────┴────┴────┴────┴────┘
             └──────── A0's result ready ──┘
                 A has 4 cycles to finish before A1 needs it
```

Same rule, better numbers: distance 4 ⇒ **II = 1** *and* the recurrence gets 4
cycles, so the clock can go up. Measured: **160 → 293 MHz, II = 1.**

**Each lane still runs at ¼ speed — that is the "C-slow" name (C times slower per
stream) — but 4 lanes finish together, so aggregate throughput is 1 symbol/cycle.**

## 3. The bit that makes it different from replication
| | what gets duplicated | what is shared |
|---|---|---|
| **Replication** (K coders) | *everything* — state **and** arithmetic | nothing |
| **C-slow interleaving** | only the **state** (low/high/context per lane) | **one** arithmetic datapath |

That is the whole trade, and it predicts our result:

- Arith coder — arithmetic is **expensive** (a 17×12 multiply, a DSP per lane).
  Sharing it wins big: **18% LUT vs 71%** for the same throughput class.
- M-coder — arithmetic is **cheap** (a table lookup, no DSP). There is nothing
  worth sharing, and interleaving still pays for per-lane context memory, so plain
  **replication wins** (31.1 vs 22.6 M sym/s on fabric).

> **Slide-worthy sentence:** *interleaving buys clock speed and area by sharing an
> expensive datapath among independent streams; it is worthless when the datapath is
> already cheap.*

## 4. The knob we swept
`GROUPS` = how many physical shared pipelines; `LANES/GROUPS` = dependence distance.

| GROUPS | distance | Fmax | note |
|---|---|---|---|
| 8 | 2 | 132–160 MHz | too little slack, clock suffers |
| **4** | **4** | **293 MHz** | **sweet spot** |
| 2 | 8 | 402 MHz | clock great, but 2× the cycles |

## 5. Where this comes from (references for the slide)

**The technique — C-slow retiming**
- N. Weaver, Y. Markovskiy, Y. Patel, J. Wawrzynek, *"Post-Placement C-slow Retiming
  for the Xilinx Virtex FPGA"*, FPGA 2003. The FPGA-specific paper; has the clearest
  diagrams of C-slowing a circuit. (Berkeley BRASS group — search "C-slow retiming
  Weaver FPGA 2003 pdf".)
- C. Leiserson & J. Saxe, *"Retiming Synchronous Circuitry"*, Algorithmica 6 (1991).
  The foundational retiming theory that C-slow builds on.

**The scheduling rule (why distance sets the II floor)**
- B. R. Rau, *"Iterative Modulo Scheduling"*, MICRO-27 / HP Labs TR, 1994 — the
  recurrence-constrained II bound `II ≥ delay/distance`.
- AMD/Xilinx *Vitis HLS User Guide (UG1399)*, "Managing Loop-Carried Dependencies"
  and `#pragma HLS DEPENDENCE` — the tool-level view of the same thing.

**The same idea under other names (useful for intuition, and good slide fodder)**
- **Barrel processors / interleaved multithreading**: Denelcor HEP, Cray/Tera MTA,
  Sun UltraSPARC T1 "Niagara" — a CPU switches thread every cycle so each thread's
  latency is hidden. C-slow is exactly this, applied to a datapath.
- **GPU warp interleaving** — same principle: many independent threads hide a long
  pipeline.

*Search terms that give good diagrams:* "C-slow retiming", "barrel processor
pipeline diagram", "modulo scheduling recurrence II", "loop-carried dependence
distance II".

## 6. Suggested slide layout
- **Left:** the "distance 1" picture — one long chain that must fit in a cycle.
- **Right:** the interleaved timeline `A B C D A B C D` with an arrow showing A's
  result arriving just in time.
- **Bottom:** the one-line rule `II ≥ delay / distance`, and the result
  **160 → 293 MHz at II=1, 18% LUT vs 71%**.
- **Punchline:** *share the expensive part, replicate only the state — and only when
  the expensive part is actually expensive.*
