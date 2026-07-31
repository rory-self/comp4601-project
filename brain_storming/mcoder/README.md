# M-coder: CABAC arithmetic engine to replace the V5 exact coder

Iteration 3 of the arithmetic coder. Same algorithm class as
[best_hls/arith5.cpp](../best_hls/arith5.cpp) — adaptive binary arithmetic
coding over an 8-level bit-tree, K independent streams — with the interval
arithmetic swapped for the H.264/AVC CABAC "M-coder" engine.

**Status: Phase 1 and Phase 2 complete.** The kernel synthesises with **II=1 on
the coding loop, zero timing violations, and zero DSPs**, and C/RTL
co-simulation passes losslessly. It closes at **6.0 ns (167 MHz)**, not the
5 ns V5 uses — see [Timing](#phase-2--hls-port). Phase 3 (K sweep) results are
in `results/mcoder_hls_sweep.csv`.

Two things worth knowing up front:

- **V6 never closed timing.** `pipelined_hls/arith6.cpp` sits at **−1.77 ns with
  4 DSPs and 25% LUT**, and its commit says so ("improving timing violation(not
  solved)"). The last design that closed timing and ran on the board is V5, at
  83.7 cycles/byte. V7 beats V6 on timing, area, and throughput at once.
- **The 6.0 ns clock is a deliberate trade**, chosen over spending more effort
  to reach 5 ns. At 8 cycles/byte and 167 MHz the coder is still far ahead of
  V5's 83.7 cycles/byte at 200 MHz.

## Why this is faster

| | V5 exact coder | M-coder |
|---|---|---|
| Interval split | `split = (range * prob) >> 12` — 17×12 multiply | `rLPS = rangeTabLPS[state][qIdx]` — 256-byte ROM read |
| Renormalisation | `while` loop, 1 bit/iteration, data-dependent trip count (bounded 32) | closed form: one barrel shift + a prefix-AND, no iteration |
| Model update | `prob += (4096-prob) >> 5` / `prob -= prob >> 5` | `state = transIdxMPS[state]` / `transIdxLPS[state]` — two 64-byte ROMs |
| Context width | 12-bit probability | 6-bit state + 1-bit MPS = 7 bits |
| Range width | 16 bits | 9 bits |

Total ROM: 256 + 64 + 64 = **384 bytes**, one BRAM or LUTROM. Multiplies per
byte drop from 9 to **0**, so the DSPs V5 burned (6 per instance, 24 at K=4)
are freed and the multiply leaves the critical path.

The renorm change is the one that matters for cycles. V5's renorm loop carries
a data-dependent `break`, so HLS schedules it sequentially and the bin body
cannot pipeline. Here it has no loop at all: the range update is a single
barrel shift, and all eight possible bit-emit classifications are produced in
parallel by a prefix-AND (derived in
[Timing](#timing-185-ns--closes-at-60-ns)). Straight-line logic, fixed latency,
so the coding loop pipelines at II=1.

## Phase 1 results

Corpus: 6 synthetic cases (repetitive / text / random / all-zero / alternating
/ tiny) plus `../demo/image.pgm` split into 4 KB blocks — **23 cases, 79,849
bytes**. Only stable inputs: an earlier version used this project's own sources
as corpus data, which made every number move whenever the code was edited.

```
make            # K=1, 8 bins/byte
make isolate    # K=1, 9 bins/byte — identical bin structure to V5
./sweep.sh      # K = 1..16, both bin structures -> results/mcoder_sweep.csv
```

### Round-trip

**23/23 lossless**, at every K in {1, 2, 4, 8, 16} and in both bin structures,
and **6/6 in RTL co-simulation**.
The decoder in [mcoder_dec.cpp](mcoder_dec.cpp) is an independent
implementation of H.264 §9.3.3.2 against the same tables, and the encoder
follows §9.3.4.2/9.3.4.5/9.3.4.6 including `firstBitFlag` and the 2-bit flush.

### Cycles per byte

Work counters, K=1, combined corpus:

| metric | V5 | M-coder |
|---|---|---|
| bins / byte | 9.000 | 8.000 |
| renorm steps / byte | 4.401 | 4.283 |
| **multiplies / byte** | **9.000** | **0.000** |
| variable-length work / byte | 2.071 pending-bits | 0.0019 stalls |
| modeled cycles / byte | 15.47 | **8.00** |

The model is stated in the header of [mcoder_test.cpp](mcoder_test.cpp) so it
can be argued with. V5: one cycle to update the interval, one per renorm
iteration executed, one per bit drained by the `while (pending)` loops.
M-coder: one cycle per bin at II=1, plus `stalls` — bins whose carry drain has
to retire more than one output byte, i.e. more than its single slot on the
output port.

**The measured stall rate is 0.0019/byte and the longest deferred-carry run
across the whole corpus is 17 bits.** That was the thing that had to be checked
before committing to this design. (The final kernel no longer depends on that
bound at all — see [the packer](#the-packer-was-the-real-bottleneck).)

Against hardware: V5 measured **83.7 cycles/byte at K=1**
([results/sweep_big_results.csv](../results/sweep_big_results.csv)). The op
model only accounts for 15.47 of those — the other ~68 are schedule overhead
from the un-pipelined bin body, which is exactly what this rewrite removes.
**83.7 → 8.00 would be 10.5×.** Phase 2 confirmed the II=1 in synthesis, so the
coder does reach 8 cycles/byte — but see Phase 3: end-to-end the AXI copy loops
cap the real gain at about 2×.

### Compression cost

The expectation going in was a 0.1–0.3% loss from quantising probability to 64
states and range to 4 buckets. That is not what happened.

`results/mcoder_sweep.csv`, size delta vs V5 (negative = M-coder smaller):

| K | 8 bins/byte | 9 bins/byte (engine isolated) |
|---|---|---|
| 1 | −2.55% | −1.73% |
| 2 | −2.96% | −2.00% |
| 4 | −3.74% | −2.51% |
| **8** | **−5.12%** | −3.51% |
| 16 | −7.08% | −4.79% |

Two separate effects, which the `isolate` build keeps apart:

1. **Bin structure (−0.8 to −2.3%).** V5 codes a continuation flag before every
   symbol: 9 bins/byte. Carrying the raw length in the header instead gives 8
   bins/byte — an 11% cut in bins that has nothing to do with the engine.
2. **The engine itself (−1.7 to −4.8%).** The M-coder is *better* than the
   exact coder on this corpus, and the gap widens with K. CABAC's 64-state FSM
   has a tuned, non-uniform adaptation trajectory that converges faster from
   the initial state than V5's fixed `>>5` exponential. Every chunk restarts
   its model from scratch, so more chunks means more time spent converging, and
   V5 pays more for that than the M-coder does.

That last point matters for Phase 3. V5's K-way compression penalty was the
thing capping useful K — 55.08% → 66.05% going from K=1 to K=16, **+11.0
points**. The M-coder pays **+7.7 points** over the same range, about 30% less.
At the K=8 operating point the M-coder is **5.1% smaller than V5 while also
being faster**.

The one place the M-coder loses badly is highly skewed data (figures from the
`make isolate` build, so bin structure is identical and only the engine differs):

| case | V5 | M-coder | delta |
|---|---|---|---|
| all-zero (4 KB) | 2.03% | 3.81% | **+87.9%** |
| repetitive | 22.71% | 24.66% | +8.6% |
| alternating | 15.14% | 16.48% | +8.9% |
| text | 59.40% | 60.45% | +1.8% |
| random | 101.32% | 102.00% | +0.7% |

This is a real, structural property, not a bug — and it doubles as evidence the
tables are correct. CABAC floors p_LPS at 0.01875 (state 62; state 63 is the
reserved terminate state and is unreachable because `transIdxMPS[62] == 62`),
so the MPS costs `-log2(1-0.01875) = 0.0273` bits/bin → 9 bins × 0.0273 / 8 =
3.07% of input, and 3.81% measured. V5's `prob -= prob >> 5` saturates near
4065/4096, floor p_LPS ≈ 0.0076, giving 1.24% predicted against 2.03%
measured. Both land where theory says, ~2.5× apart in representable skew.

If the real workload is dominated by near-constant regions, this coder gives up
real ratio there. On the demo image and on text it does not.

## Files

| file | role |
|---|---|
| [mcoder_tables.h](mcoder_tables.h) | H.264 Tables 9-44 / 9-45, 384 bytes, with the p_LPS sanity model |
| [mcoder.h](mcoder.h) | the engine — `mc_code_bin`, `mc_renorm_low`, the packer primitives, decoder, optional counters |
| [mcoder_enc.cpp](mcoder_enc.cpp) | K-way encoder top level, mirrors `arith5.cpp` |
| [mcoder_dec.cpp](mcoder_dec.cpp) | verification decoder (host side only, never in the kernel) |
| [mcoder_test.cpp](mcoder_test.cpp) | round-trip, ratio, work counters, cycle model |
| [ref/v5_prof.cpp](ref/v5_prof.cpp) | instrumented copy of V5 for side-by-side; `best_hls/` stays the source of truth |
| [hls/mcoder_hls.cpp](hls/mcoder_hls.cpp) | the kernel — dataflow split, AXI interface, pragmas |
| [hls/mcoder_hls_test.cpp](hls/mcoder_hls_test.cpp) | csim/cosim testbench, self-checking via the real decoder |
| [hls/hls_config.cfg](hls/hls_config.cfg) | part, 6.0 ns clock, top = `mc_encode` |
| [hls/sweep_hls.sh](hls/sweep_hls.sh) | K sweep: synthesis + cosim → `results/mcoder_hls_sweep.csv` |
| [hls/mcoder_board.cpp](hls/mcoder_board.cpp) | board top-level, signature-compatible with V5's `arith_kernel` |
| [hls/hls_board.cfg](hls/hls_board.cfg) | board build → `arith_kernel.xo` |
| [host/mc_host.cpp](host/mc_host.cpp) | XRT host — links the real decoder, `-f` for real data, `make sim` for a boardless test |

Counters compile out entirely without `-DMC_PROFILE`, so the HLS build never
sees them.

The engine in [mcoder.h](mcoder.h) is shared: `mc_code_bin` and
`mc_renorm_low` are the same code in the software build and in the kernel, so
the Phase 1 corpus (29 cases, K in {1..16}) validates the exact arithmetic the
hardware runs. Only the packer differs — software packs immediately, the kernel
flattens it into a pipelined stage — and cosim checks that against the same
independent decoder.

## Phase 2 — HLS port

Kernel: [hls/mcoder_hls.cpp](hls/mcoder_hls.cpp), config
[hls/hls_config.cfg](hls/hls_config.cfg). Target `xck26-sfvc784-2LV-c`, the
same part V5 and V6 use.

```
source /tools/Xilinx/2025.2/Vitis/settings64.sh
cd hls
vitis-run --mode hls --csim  --config hls_config.cfg --work_dir work_k1   # functional
v++ -c     --mode hls        --config hls_config.cfg --work_dir work_k1   # synthesis
vitis-run --mode hls --cosim --config hls_config.cfg --work_dir work_k1   # RTL + cycles
./sweep_hls.sh                                                           # K = 1..16
vitis-run --mode hls --package --config hls_config.cfg --work_dir work_final  # -> mc_encode.xo
```

`hls_config.cfg` defaults to **K=8**, the operating point chosen in Phase 3.

### Result

| | V5 (on board) | V6 (`arith6.cpp`) | **V7 (this)** |
|---|---|---|---|
| Timing | closes @ 5 ns | **−1.77 ns, unsolved** | **closes @ 6.0 ns** |
| Coding loop | not pipelined | II=1 flat FSM | **II=1** |
| Work per byte | ~9.3 cyc/bin | ~13 units | **8 bins = 8 cyc** |
| DSP | 6 / instance | 4 | **0** |
| LUT @ K=1 | — | 30115 (25%) @ K=4 | 11711 (9%) |
| Round-trip | — | — | cosim PASS, 6/6 |

### The two-stage split

The engine is cut in half at the point where the work stops being fixed-latency:

- **`mc_bin_stage`** owns `low`/`range`/contexts. One bin per cycle, one stream
  write per bin. II=1.
- **`mc_pack_stage`** owns the deferred-carry count and the byte packer.

This is not optional. A first attempt kept everything in one loop; the
deferred-carry drain is a variable-trip loop, HLS refused to pipeline around it
("contains subloop(s) that are not unrolled or flattened"), and the result was
**128 cycles/bin at 52% LUT**.

The two halves cannot be split naively either, because one bin can emit several
carry runs and several stream writes per iteration force II > 1. What makes it
work is that **the packer never needs `low`**: what a renorm step emits is fully
determined by its *classification* (E1 emits 0 then drains, E2 emits 1 then
drains, E3 defers), so a bin's ≤8 steps pack into one fixed-width token and the
packer maintains `outstanding` itself. One token per bin, one write, II=1.

### Timing: 18.5 ns → closes at 6.0 ns

Every fix was a width or depth reduction on the loop-carried path, and each was
verified bit-identical against the Phase 1 corpus before moving on.

| change | measured effect |
|---|---|
| Baseline: `uint32_t` state, 8-deep iterated renorm | path **18.5 ns** |
| Narrow `low`/`range` to 10/9 bits via explicit masks | **−2.66 ns** slack |
| ROM addressed by `st` alone (`mc_rlps4`), so the table lookup leaves the loop-carried `range` path | **−1.57** |
| Replace `w >> (q<<3)` with a 4:1 mux over constant shifts | **−0.97** |
| Compute MPS/LPS candidates in parallel; MPS renorm is provably 0–1 bits | **−0.79** |
| Closed-form renorm classify (prefix-AND instead of 8 sequential steps) | **−0.61** |
| Move to 6.0 ns + flatten/narrow/un-fold the packer | **closes, 0 violations** |

Two of these are worth calling out because they are not obvious:

**The MPS renorm is provably 0 or 1 bits.** `q` partitions `range` into
`[256,319] [320,383] [384,447] [448,511]`, and rLPS is largest at state 0, so
the smallest `range - rLPS` per quantile is `256−128, 320−176, 384−208,
448−240` — all ≥ 128. A 9-bit value ≥ 128 needs at most one doubling. That
deletes the priority encoder and barrel shifter from the MPS path entirely.
`mc_prof.sm_violations` guards the assumption so a future table edit cannot
silently break it.

**The 8-deep renorm chain has a closed form.** The step recurrence
`L' = (L<<1) & (b9 ? 0x3FF : 0x1FF)` only ever touches bit 9, so

```
L_i[8:0] = (L_0 << i) & 0x1FF          one barrel shift
L_i[9]   = L_0[9] & L_0[8] & ... & L_0[9-i]    a prefix-AND
```

All eight classifications therefore fall out of one depth-3 AND tree plus fixed
wiring, in parallel, instead of eight sequential shift-mask-select levels.

### Why 6.0 ns and not 5 ns

At 5 ns the coder path is 5.61 ns. Closing the last 0.61 ns is possible — the
next step would be precomputing `sL` and the normalised LPS range into the ROM,
which deletes the priority encoder from the LPS path too — but 6.0 ns was taken
as a deliberate stopping point.

Above 6.5 ns the *coder* is comfortable (+0.21) but the AXI byte-copy loops
(`Split`/`Concat`) start reporting −0.01, because HLS repacks them when given a
longer clock. Those loops are structurally identical to V5's, which close at
5 ns, so that is a scheduling artifact rather than a real limit. 6.0 ns is the
window where every module meets: coder **+0.12**, packer **+0.42**, copy loops
**0.00**.

### The packer was the real bottleneck

Cosim, not synthesis estimates, caught this. The first working version measured
**25 cycles/byte** even though the coder was at II=1 — `mc_pack_stage`'s nested
`Steps`→`Carry` loops cost ~10 cycles of loop control per step. Flattening it
into a single pipelined loop that does exactly one unit of work per cycle
(consume one step, or emit one carry bit) took the packer from **2,762,242 to
33,284 cycles**, matching the coder. Total cosim time went 130,602 → 73,578
cycles.

That flattening also *removed* an assumption: carry runs are now emitted one bit
per cycle for as long as they last, so an arbitrarily long run is correct rather
than relying on the measured 17-bit maximum.

## Phase 3 — K-way scaling

`results/mcoder_hls_sweep.csv`, produced by [hls/sweep_hls.sh](hls/sweep_hls.sh).
Cycles are **measured by cosim** on the testbench corpus (5130 bytes over 6 data
profiles), so they include the AXI copy loops and the packer, not just the coder.

Two context-storage variants are swept: `ctxreg` (partitioned register file)
and `ctxram` (distributed RAM — see
[Resource balance](#resource-balance-why-dsp0-and-bram-is-low)).
**Every one of the ten configurations closes timing with II=1.**

| variant | K | cosim cycles | cyc/byte | LUT | LUT % | FF | BRAM | DSP |
|---|---|---|---|---|---|---|---|---|
| ctxreg | 1 | 73578 | 14.34 | 11243 | 9% | 4936 | 13 | 0 |
| ctxreg | 2 | 46906 | 9.14 | 19946 | 17% | 8089 | 16 | 0 |
| ctxreg | 4 | 35292 | 6.88 | 35530 | 30% | 13609 | 22 | 0 |
| ctxreg | 8 | 32694 | **6.37** | 67349 | 57% | 24792 | 42 | 0 |
| ctxreg | 16 | 39505 | 7.70 | 131000 | **111%** — does not fit | 47181 | 82 | 2 |
| ctxram | 1 | 75120 | 14.64 | 8049 | 6% | 3377 | 13 | 0 |
| ctxram | 2 | 48448 | 9.44 | 13558 | 11% | 4971 | 16 | 0 |
| ctxram | 4 | 36834 | 7.18 | 22754 | 19% | 7373 | 22 | 0 |
| **ctxram** | **8** | **34236** | **6.67** | **41797** | **35%** | 12320 | 42 | **0** |
| ctxram | 16 | 41047 | 8.00 | 79896 | 68% | 22237 | 82 | 2 |

**`ctxram` at K=8 is the operating point** and the default in
`hls_config.cfg`: 35% LUT instead of 57% for 4.7% more cycles.

Two things the sweep settles. K=16 does not fit at all with register contexts
(111% LUT), and even once `ctxram` makes it fit (68%) it is *slower* than K=8 —
8.00 against 6.67 cyc/byte. And the returns collapse well before that: K=1→2
buys 36%, K=2→4 buys 24%, K=4→8 buys 7%. That is the AXI ceiling, not the
coder.

### The coder is no longer the bottleneck

This is the main thing the sweep says, and it changes what to work on next.

At K=8 the coder needs 8/8 = 1 cycle per byte. Measured is 6.37. The missing
~5.4 cycles/byte are the `Split` and `Concat` loops, which copy bytes over an
**8-bit-wide** `m_axi` port one byte per cycle and **do not divide by K** — they
move the same total bytes no matter how many coders run. That is exactly why
the curve flattens from K=4 (6.88) to K=8 (6.37) and then reverses at K=16,
where the extra area buys nothing and costs routing.

So the algorithm work is done, and further speedup is a *memory-path* problem,
not a coder problem: widen `gmem` (the interface reports `8 -> 8` with
`Max Widen Bitwidth 512` available), or stream input straight into the K coders
instead of staging through `buf[][]`.

### Projected throughput

V5 measured **13.29 M symbols/s on fabric** at K=8 (200 MHz). Cross-check: V5's
cosim figure of 15.5 cyc/byte at K=8 and 200 MHz gives 12.9 M symbols/s, which
matches the board number closely — so cyc/byte from cosim is a fair predictor.

Applying the same conversion to V7 at K=8 and 167 MHz gives
`167e6 / 6.37 ≈ 26.2 M symbols/s`, about **2.0× V5's measured board
throughput**. That is a projection from cosim, not a board measurement — the
board test has not been run yet.

Worth being clear about the shape of that number: the *coder* got ~10× faster
(83.7 → 8 cycles/byte single-stream), but end-to-end only ~2×, because the
byte-wide AXI copies now dominate. Fixing those is where the remaining speedup
is.

### Resource balance: why DSP=0 and BRAM stays low

**LUTRAM is not BRAM.** They are different physical resources, and the fix
below uses LUTRAM, so the BRAM column does not move at all:

| | BRAM (block RAM) | LUTRAM (distributed RAM) |
|---|---|---|
| what it is | dedicated 36 Kb hard blocks (144 on this part) | the 64-bit SRAM cell inside a SLICEM LUT6 |
| counted as | `BRAM` | **`LUT`** |
| read | synchronous, ≥1 cycle | **asynchronous / combinational** |
| good for | KB-scale buffers | small, latency-critical tables |

The asynchronous read is why LUTRAM suits the context array: the
read-modify-write still resolves fast enough to pipeline. And because LUTRAM is
*made of* LUTs, the saving shows up as a smaller LUT count, not as BRAM usage —
it went 67349 → 41797 rather than to near zero.

Using true BRAM for the contexts was measured too, and is **not** worth it:

| K=8, contexts in | LUT | BRAM | cyc/byte |
|---|---|---|---|
| registers | 67349 (57%) | 42 (14%) | 6.37 |
| **LUTRAM** | **41797 (35%)** | **42 (14%)** | 6.67 |
| BRAM | 41573 (35%) | 50 (17%) | 6.67 |

8 extra BRAM blocks to save ~200 LUTs. The array is 256 × 8 bits = 2 Kb, and a
BRAM18 holds 18 Kb, so ~89% of each block would sit idle. LUTRAM is simply the
right-sized resource for a table this small.

BRAM stays at 14% because what actually lives there is the `buf[][]`/`cout[][]`
staging arrays — and those are sized by the chunk buffers, not by anything the
coder does.

The design is **LUT-bound**; DSP and BRAM are near-idle. That is worth
unpacking, because it is half deliberate and half a missed opportunity.

**DSP=0 is the point, not waste.** V5 spent 6 DSPs per instance on
`range * prob >> 12`, and that multiply was the critical path. The M-coder
replaces it with a ROM read, so there is no multiply anywhere in the design.
There is no useful way to spend the DSPs back: a DSP48E2 is a 27×18 multiplier
/ 48-bit ALU, and this datapath is 9–10 bits wide. Forcing a 9-bit subtract
into a DSP adds latency and saves no LUTs worth having. The DSPs are simply not
the right shape for this problem — and freeing them is a *result*, since they
are now available to whatever else shares the device.

**The missed opportunity was the context array, and it was worth ~22% of the
device.** Measured at K=8, `mc_bin_stage_Pipeline_Bins` was **6759 LUT and 2143 FF per
coder** — about 90% of the whole design — while `mc_pack_stage` was 630 LUT.
The 2143 FF is 256 contexts × 8 bits: `#pragma HLS ARRAY_PARTITION complete`
had built a 256-entry register file, whose 256:1 read mux and 256-way write
decode are what the LUTs were paying for.

Binding that array to distributed RAM instead (`-DMC_CTX_LUTRAM`) collapses it:

| K=8 | contexts in registers | contexts in LUTRAM |
|---|---|---|
| LUT | 67349 (57%) | **41797 (35%)** |
| FF | 24792 | 12320 |
| `Bins` LUT / coder | 6759 | **800** |
| cyc/byte | 6.37 | 6.67 |
| II | 1 | 1 |
| timing | clean | clean |

**−38% LUT for +4.7% cycles**, so it is the default in `hls_config.cfg`.

The catch is that LUTRAM makes the read-modify-write take two cycles, which
forces II=2 unless HLS is told how far apart two accesses to the same context
can be. It is exactly 8 bins, and that is provable: within one byte `ctx` walks
strictly down the bit-tree so its 8 values are distinct, and across bytes, bit
position `j` only ever addresses `[2^j, 2^(j+1)-1]`, and those ranges are
disjoint. So a context recurs only at the same bit position of the next byte.

`#pragma HLS DEPENDENCE ... distance=8` states that, and HLS cannot check it —
so it is checked two other ways: `mcoder_test` tracks the actual distance
between uses of every context over the whole corpus and fails on any violation
(`ctx_dist_violations`), and cosim would fail the round-trip if a context were
ever read stale, since the decoder updates its model strictly sequentially.

**This does not buy throughput, though.** With the area freed, K=16 fits for the
first time (68% LUT, timing clean, II=1) — and is *slower*: 8.00 cyc/byte
against K=8's 6.67. That is the AXI ceiling again, and it is the clearest
evidence that more coders are not the answer. The win here is area and routing
headroom for the memory-path work, not speed.

### Compression cost of K

From `results/mcoder_sweep.csv` (Phase 1). The M-coder's per-chunk ratio penalty
is ~30% smaller than V5's — V5 pays +11.0 points going K=1→16, the M-coder
+7.7 — so at the K=8 operating point the M-coder is **5.1% smaller** than V5
while also being faster.

## Board integration — can you reuse the V5 host?

**Mostly yes: the XRT plumbing is unchanged, but the host-side decoder must be
replaced.**

To make that true, [hls/mcoder_board.cpp](hls/mcoder_board.cpp) exports a top
level deliberately identical in shape to V5's `board/arith_board.cpp`:

```c
void arith_kernel(const byte in[MAX_IN], int n, byte out[MAX_OUT], int out_len[1])
```

Same kernel name, same four arguments, same `gmem0/gmem1/gmem2` bundles. So in
[board/host.cpp](../board/host.cpp) these all keep working as-is:

- `xrt::kernel(device, uuid, "arith_kernel")` — name matches
- `krnl.group_id(0) / (2) / (3)` — argument indices match (`in`, `out`, `out_len`)
- the buffer sizes, `sync()` calls, and the timing loop
- `board/link.cfg`'s `nk=arith_kernel:1:arith_kernel_1` — no edit needed

Build it with:

```
cd hls && v++ -c --mode hls --config hls_board.cfg --work_dir work_board
vitis-run --mode hls --package --config hls_board.cfg --work_dir work_board
```

→ `arith_kernel.xo` (verified: cosim PASS, 0 timing violations, II=1).

### The host

[host/mc_host.cpp](host/mc_host.cpp) is the M-coder host, built on V5's XRT
structure (same kernel lookup, same buffer objects, same chrono-around-
enqueue+wait profiling) with three changes:

```
cd host
make SYSROOT=<xrt-sysroot>          # aarch64 board build -> mc_host_arm
make sim                            # host-logic test on this machine, no XRT
./mc_host_arm -x mcoder.bin -f image.pgm -N 4095 -n 2000
```

1. **It links `../mcoder_dec.cpp` rather than carrying its own decoder.** V5's
   host inlined a copy with a comment that its constants "must match the kernel
   / arith3.h" — which is exactly how a decoder silently drifts from its
   encoder. There is now one decoder in the project, the same one Phase 1 and
   cosim validate against.
2. **`-f <file>` runs real data.** V5's host measured `'a'+(i%7)`, which
   compresses to 41.2% where the demo image gives 67.4%. Throughput is
   content-independent (8 bins/byte regardless), but the *ratio* is not, and
   the synthetic number flatters it.
3. **`make sim`** swaps the kernel call for the software encoder so the
   argument handling, file IO, verification and reporting can be exercised
   without a board. It does not test XRT.

It also reports implied cycles/byte at the kernel clock, so a board run can be
compared directly against the 6.67 cyc/byte cosim prediction — a large gap
means host/AXI overhead rather than a coder problem.

### A decoder hardening this turned up

Writing the host exposed a real bug. `mc_decode` took the per-chunk raw length
straight from the container header and wrote that many bytes to the caller's
buffer. Inside the test harness the header always came from our own encoder, so
it was always sane — but the host reads it out of *device* memory, and a wrong
xclbin (V5's, say) yields an arbitrary header. AddressSanitizer confirms the
overflow on the unfixed code:

```
ERROR: AddressSanitizer: global-buffer-overflow in mc_decode_chunk(...)
```

`mc_decode` now validates the header before using it. Re-checked with 200,000
random streams plus a deliberately V5-shaped header under ASan: all rejected,
zero overruns, and the encoder output is unchanged.

### Two things you must change

**1. The decoder — this one will bite silently.** `board/host.cpp` carries its
own `dec_kway()` and a comment saying the constants "must match the kernel /
arith3.h". They no longer do, in two independent ways:

| | V5 | V7 |
|---|---|---|
| container | `[K x uint16 comp_len]` | `[K x {uint16 raw_len, uint16 comp_len}]` |
| termination | per-symbol continuation flag | raw length in the header |
| model | 12-bit probability, `>>5` update | 6-bit state + MPS, ROM transitions |

Running V5's `dec_kway()` on a V7 stream produces garbage and reports a
round-trip mismatch — it will not tell you the decoder is the wrong one. Drop in
[mcoder_dec.cpp](mcoder_dec.cpp) instead and call `mc_decode(out, clen, dec)`;
it is standalone (no HLS headers) and already the reference the whole project
is validated against.

**2. The clock.** V7 closes at 6.0 ns, so the link step needs ~167 MHz, not
V5's 200 MHz. Throughput projections in this README already account for that.

## Next steps

1. **Board test at K=8.** `mc_encode.xo` is built and verified (cosim PASS,
   0 timing violations, 6.37 cyc/byte) with the same `m_axi` + `s_axilite`
   interface as V5/V6, so it drops into the existing link flow. One thing to
   watch: the 6.0 ns clock means the link step needs a matching kernel
   frequency (~167 MHz), not the 200 MHz V5 uses.
2. **Widen the AXI path.** This is now the bottleneck, worth more than any
   further coder work.
3. **Optional, to reach 5 ns:** precompute `sL` and the normalised LPS range
   into the ROM, deleting the priority encoder from the LPS path as was already
   done for MPS. Estimated to recover the last 0.61 ns.
