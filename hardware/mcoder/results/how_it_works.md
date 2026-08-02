# How the M-coder works, and why it is fast

Design notes for `hardware/mcoder`. The measurements are in `synth_results.md`
and `onfabric_result.txt`; this file is the reasoning behind the design.

Naming note: this design replaces the exact coder used by `replication_full`.
Where the text below says "V5", it means `replication_full`.

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

The one place the M-coder loses is highly skewed data (figures from the
`make isolate` build, so bin structure is identical and only the engine
differs).  Read the last column carefully -- the relative deltas are large but
they sit on top of outputs that are already tiny:

| case | V5 | M-coder | delta (relative) | delta (% of input) |
|---|---|---|---|---|
| all-zero (4 KB) | 2.03% | 3.81% | +87.9% | **+1.78 pp** (83 -> 156 B) |
| repetitive | 22.71% | 24.66% | +8.6% | +1.95 pp |
| alternating | 15.14% | 16.48% | +8.9% | +1.34 pp |
| text | 59.40% | 60.45% | +1.8% | +1.05 pp |
| random | 101.32% | 102.00% | +0.7% | +0.68 pp |

So the worst case costs under 2 percentage points of the input size, and it
costs it where both coders are already compressing 26-49x.  "+88%" is the
honest relative figure but a misleading headline.

This is a real, structural property, not a bug — and it doubles as evidence the
tables are correct. CABAC floors p_LPS at 0.01875 (state 62; state 63 is the
reserved terminate state and is unreachable because `transIdxMPS[62] == 62`),
so the MPS costs `-log2(1-0.01875) = 0.0273` bits/bin → 9 bins × 0.0273 / 8 =
3.07% of input, and 3.81% measured. V5's `prob -= prob >> 5` saturates near
4065/4096, floor p_LPS ≈ 0.0076, giving 1.24% predicted against 2.03%
measured. Both land where theory says, ~2.5× apart in representable skew.

If the real workload is dominated by near-constant regions, this coder gives up
real ratio there. On the demo image and on text it does not.

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

### It runs at 200 MHz, not 167 — the clock fallback was never needed

Worth correcting, because the earlier reasoning in this file was wrong about
what the 6.0 ns constraint does. The KV260 platform exposes only **fixed**
clocks (100 / 199.998 / 400 MHz), so `clock=6.0ns` never set the hardware
clock at all — it only steered HLS scheduling. The kernel is clocked at
**5.000 ns**, and post-route timing closes with room:

```
WNS 0.754 ns   TNS 0.000   0 failing endpoints of 71960
```

So HLS's -0.61 ns estimate at 5 ns was pessimistic — it reserves 12.5% clock
uncertainty and estimates routing before placement. The real path is 3.996 ns
with **2 logic levels**, 56% of it routing. Choosing 6.0 ns cost nothing, but it
was not the thing that made this work.

The post-route critical path is also informative: it lands in
`mc_encode_Pipeline_VITIS_LOOP_273_2` — the `Concat` output copy loop — **not**
in the coder.

### Where the time actually goes

At K=8 a 4095-byte buffer splits into 512-byte chunks, so the coder does
512 x 8 = **4096 cycles** of the measured 26,317. The coder is **15.6% of the
runtime**; the other 84% is moving bytes.

That is the whole story of this iteration in one number. The engine got ~10x
faster (83.7 -> 8 cycles/byte single-stream) and the system got 2.34x, because
the bottleneck moved to the byte-wide `m_axi` copies, which do not divide by K.
It is also why K=16 fits after the LUTRAM change yet runs *slower*.

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

### The one image class where V5 wins

[demo/text_page.pgm](demo/text_page.pgm) (regenerate with
[demo/gen_text_page.py](demo/gen_text_page.py)) is bilevel "scanned text" — pure
0/255 with long white runs. It is the worst case for the M-coder, and it shows
the tradeoff crossing over as K changes:

| | V5 | M-coder | delta |
|---|---|---|---|
| K=1 | 5488 | 6111 | **+11.35%** — V5 wins |
| K=2 | 6306 | 6613 | **+4.87%** — V5 wins |
| K=4 | 7654 | 7474 | −2.35% — crossover |
| **K=8** | 10234 | 9126 | **−10.83%** — M-coder wins |

The mechanism is checkable. V5's `prob += (4096-prob)>>5` only stops once
`4096-prob < 32`, so p_LPS floors at 31/4096 = 0.0076; reaching it from 0.5
takes roughly `32*ln(2048/31) ~= 134` consecutive identical bits **in one
context**. The M-coder plateaus earlier at 0.01875. So V5 wins exactly where a
single context sees runs longer than ~134 bits — long white margins in bilevel
text do that; photographs and gradients do not.

Which is why the advantage evaporates with chunking: at K=8 the 4096-byte block
splits into 512-byte chunks, every chunk restarts its model, and the deep
contexts see only a handful of samples each. Nothing reaches the floor, so what
decides the result is convergence speed from the initial state — and CABAC's
tuned FSM wins that comfortably.

**At the shipped K=8 there is no realistic image where V5 wins.** This was
searched for, not assumed: 99.9%-pure constant fields, 99%/95% variants, sparse
document scans, bilevel checkerboards and flat-region synthetics all lose by
8-23% at K=8. The only K=8 case favouring V5 is uniform random noise, by
**+0.05%**, where both coders *expand* the data to ~102%.

> Probability floor decides long runs; adaptation trajectory decides short
> chunks. K picks the regime — and throughput already forced us to K=8.

**Careful with which speedup you quote.** The demo reports FPGA vs *ARM running
the same M-coder* — that is the "what did acceleration buy" number (~9x). It is
a different comparison from the 2.34x vs V5 on fabric, which is "what did
iteration 3 buy over the previous accelerator". Both are real; they are not the
same claim.
