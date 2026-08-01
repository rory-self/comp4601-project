# Arithmetic coding on the KV260 — what we did, why, and what we found

COMP4601 project: accelerating **adaptive arithmetic coding** (lossless
compression) on the AMD/Xilinx **KV260** FPGA using HLS.

> **The main point.** There is no single speedup figure for arithmetic coding on
> an FPGA. The achievable acceleration is a function of the operating regime —
> the data, the architecture, the platform clock, the block size. What we did is
> *map that regime* and validate the winning design on real silicon, rather than
> quote one headline number.

All figures are measured — real board via XRT, or cycle-accurate C/RTL
co-simulation, as marked. ARM baseline = Cortex-A53 @ 1.33 GHz, **3.46 M sym/s**,
measured on the board.

---

## 1. Why arithmetic coding fights acceleration (the wall)

Arithmetic coding represents a whole message as one number in `[0,1)`: start with
the interval, and for each symbol shrink it into the sub-slice sized by that
symbol's probability. Likely symbols barely shrink it (few bits), rare symbols
shrink it a lot (more bits) — reaching the entropy limit. "Adaptive" means the
probability model *learns* as it reads. **Renormalisation** emits the settled top
bits as the interval shrinks — a *variable* number of output bits per symbol.

The wall: **encoding symbol N needs the interval from symbol N-1.** That is a
**sequential recurrence** with no way around it, so:
- **You can't parallelise a single stream** — every step waits on the previous.
- **You can't cleanly pipeline it** — renorm emits a variable number of bits per
  symbol, which a fixed-rate pipeline can't absorb (a forced `PIPELINE` pragma ran
  >14 min without converging).

So a naive FPGA port is *doomed* to lose to the CPU: the same sequential work, at
200 MHz instead of 1.33 GHz. **We measured exactly that** (naive HLS =
0.29–2.4 M sym/s, i.e. 0.08–0.69× the CPU — slower). The whole project is how to
get around this wall.

---

## 2. How we got around it — two forms of parallelism

The coder core we use is a **binary range coder** (LZMA-style): it codes one *bit*
at a time with an adaptive model, which lets us replace the costly divide with a
**bit-shift** (power-of-two total) and use a tiny adaptive bit-model over a
255-node context tree. Input is **burst-loaded** on-chip. This makes one stream
lean — but it is *still sequential*, so still ~on par with the CPU. Parallelism
then comes from one of two strategies:

- **Replication (iteration 1).** Split the input into **K independent chunks**,
  run **K complete coders side by side**. K messages' worth of work at once —
  something a single CPU can't do. Trades a little compression (each chunk resets
  its model + a small header) for ~K× throughput. This is the design we put on the
  board.
- **C-slow interleaving (iteration 2).** Instead of K full copies, run **one
  shared arithmetic pipeline** that rotates through `LANES` independent coder
  states round-robin. This raises the *recurrence distance* (a lane is revisited
  only every LANES cycles), which is enough for HLS to schedule the central loop
  at aggregate **II=1** — the same throughput class as replication but at a
  fraction of the area, keeping the full model.

**The critical path in both is the interval-update multiply** (`range * prob`,
~2.4 ns). It sits inside the recurrence, so it caps the clock — the central fact
behind most of the results below.

---

## 3. What's done (the designs)

| design | folder | coder | parallelism | status |
|---|---|---|---|---|
| naive (WNC exact, divide) | *(deleted — result kept)* | exact | single stream | baseline, proves the wall |
| **replication (V5)** | `replication_full/` | binary, full 255-ctx | K-way | **board-validated** |
| lean max-K | folded into `replication_full/LEAN_VARIANT.md` | binary, reduced 8-prob | K=31 | theory ceiling, doesn't fit |
| **interleaved (v2)** | `interleaved/` | binary, full 255-ctx | C-slow, II=1 | board-validated |

Interleaving enablers (measured, not guessed): exact-width `ap_uint` datapath
(g4 Fmax 273→293 MHz) and a **registered DSP multiply** (lifted the multiply off
the recurrence critical path → g2 reaches 402 MHz at II=1).

---

## 4. Results

### 4a. The core progression (measured)
| implementation | throughput | vs ARM |
|---|---:|---:|
| Software — ARM Cortex-A53 (baseline) | 3.46 M sym/s | 1.0× |
| Naive HLS — single stream | 0.29–2.4 M sym/s | **0.08–0.69× (slower!)** |
| **Replication K=8 — on FPGA fabric** | **13.29 M sym/s** | **3.84×** |
| Replication K=16 — co-sim | 16.8 M sym/s | 4.9× |

Replication scaling is sub-linear (K=8 → 5.4×, not 8×) because of fixed per-chunk
setup that doesn't parallelise (**Amdahl**); ~K=16 fills the chip (71% LUT).

### 4b. Input statistics — speedup is data-dependent (two ways)
Same kernel (K=8), same board, two 256×256 images:

| workload | compressed | ARM | FPGA | speedup |
|---|---|---:|---:|---:|
| smooth / compressible | 70.4% | 2.92 MB/s | 10.34 MB/s | **3.54×** |
| random / incompressible | 102.1% | 2.46 MB/s | 9.43 MB/s | **3.83×** |

Absolute throughput tracks compressibility (fewer renorm bits → faster on both
CPU and FPGA); but the speedup *ratio* moves the **opposite** way (more real work
per symbol amortises the fixed overhead → higher ratio). **The workload that
compresses best and the one that accelerates best are not the same one.**

### 4c. Simulation vs silicon — the design that won in cosim lost on hardware
| design | cosim | on fabric (200 MHz, 4 KB) |
|---|---:|---:|
| interleaved g4 | **31 M/s** (293 MHz, 18% LUT, cosim) | 11.7 M/s (3.4×) |
| K=8 replication | 12.9 M/s | **13.2 M/s** (3.8×) |

The interleaved kernel is **lossless and correct on fabric** (4095→1904 B,
byte-identical to cosim) — but slower than replication, for two reasons cosim hid:
1. **Clock cap.** The platform offers only fixed **100/200/400 MHz** clocks. g4's
   edge needed ~293 MHz; forced to 200, the headroom is thrown away.
2. **Idealised AXI.** Cosim models memory as flat 64-cycle latency. The interleaved
   kernel's I/O doesn't burst ("inferred burst reverted"), so real DDR adds ~1.46×
   to its kernel time. Replication bursts cleanly and matches its cosim.

### 4d. Block size — overhead amortisation
Fixed ~62 µs per call (XRT enqueue+wait), so throughput depends on how much you
feed per call (interleaved g4, fabric): 256 B = 3.2 M/s → 1 KB = 7.3 → 2 KB = 9.6
→ 4 KB = 11.7 → asymptote ~14.2. Fit: g4 = 62 µs + 70.5 ns/sym; K=8 = 55 µs +
62.2 ns/sym.

### 4e. Lean max-K ceiling (theory only)
K=31 reduced-model replication reaches **28.2 M/s (8.15× ARM)** in cosim but at
**95% LUT** (won't route) and worse compression (55.75% vs 46.50%). Dominated by
interleaving. Kept as an upper bound, not a design — see
`replication_full/LEAN_VARIANT.md`.

### 4f. The regime map (which design wins where)
- **On this fixed-clock board, right now:** K=8 replication (13.2 M/s, bursts I/O,
  clocks at 200).
- **On area-efficiency / cosim:** interleaving (same throughput class at ~1/5 the
  LUT, full model), and it would win on throughput given a ~300 MHz clock.
- **As a throughput ceiling:** lean max-K (28.2 M/s, but doesn't fit).

---

## 5. Conclusions (state plainly)
1. Single-stream arithmetic coding **cannot** beat the CPU; acceleration is about
   extracting parallelism from a sequential recurrence.
2. The best architecture is **regime-dependent**: replication wins on *this*
   fixed-clock board; interleaving wins on area-efficiency and would win on
   throughput with a ~300 MHz clock.
3. Speedup is **data-dependent** in two separate ways (absolute throughput vs
   ratio) — a single number is meaningless without the workload.
4. **Simulation ≠ silicon:** platform clock quantisation and un-burstable I/O
   reversed the cosim ranking on real hardware. We report the negative result as a
   finding, not a failure.

---

## 6. Remaining to-dos
- **Bursted / coalesced I/O.** The ~1.46× real-DDR tax on the interleaved kernel
  is un-burstable `m_axi`. Redesign Load/Store as coalesced/wide transfers
  (dataflow load → compute → store). Clock-independent win.
- **Kill the multiply (table-driven coding).** The interval multiply is the
  critical path. Two multiply-free routes:
  - our **"tree"/tANS idea** — precompute interval boundaries for a fixed
    histogram into a table; encode a whole byte per state transition (no multiply,
    ~1 op/byte). Requires a static model; on stationary images the compression
    cost is ~nil (both order-0).
  - a teammate's **M-coder (CABAC)** already realises this: table lookups, zero
    DSP, II=1, and it *compresses better* than our range coder — a proven starting
    point. The strongest next design is likely **his M-coder inside our
    interleaving harness** (multiply-free core + area-efficient scheduling).
- **Realise 31 M/s on fabric** would need *both* the above *and* a ~300 MHz
  platform clock (a platform rebuild, since 200/400 are the only nearby options).
