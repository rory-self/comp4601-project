# Lean max-K replication — theory only (no separate design)

We briefly explored a *maximum-throughput* variant of this same K-way replication
idea, asking: how many lanes fit on the KV260 if we trade compression for lane
count? It did **not** earn its own design — it is documented here and then set
aside, for the reasons below.

**The change.** Each lane still does adaptive binary arithmetic coding, but with
a **reduced model**: 8 probabilities (one per bit position) instead of the full
255-node context tree, and state narrowed to 16 bits. That shrinks one lane from
~4,749 LUT / 6 DSP to ~3,389 LUT / 2 DSP, which lets many more lanes fit.

**The result (post-HLS co-simulation, K=31):**

| metric | value |
|---|---|
| throughput | 28.20 M sym/s @ 200 MHz (**8.15× ARM**, theoretical) |
| LUT | **95%** (K=32 = 101%, K=64 = 196% — do not fit) |
| compression | pattern **55.75%** vs the full model's 46.50% (worse) |

**Why it's not worth a separate design:**
1. **It almost certainly won't route.** 95% LUT before the platform's own logic is
   over the practical placement ceiling; Vitis itself flags it "may fail
   placement/routing." It was never board-validated.
2. **It's dominated.** The interleaved design (see `../interleaved/`) reaches a
   similar throughput class at **18% LUT** while keeping the **full** 255-context
   model — i.e. better compression *and* far less area. Lean max-K gives up
   compression to fill the chip; interleaving gives up neither.
3. **No deployable throughput gain, no area saving** relative to the alternatives.

So it stands only as a **theoretical upper bound** on "how much coder fits on this
fabric," not as a design we ship. The full-model K-way replication in this folder
is the real, board-validated replication result.
