# Why DATAFLOW does not help the interleaved M-coder (and what would)

**Not built — ruled out by structure plus the measured loop breakdown.** Recording
the reasoning because the blocker is a *data-layout* problem, not a pragma problem,
and it generalises.

## The idea
Wrap the kernel's three phases (Load → Interleave/encode → Header/Store) in
`#pragma HLS DATAFLOW` so input staging overlaps with coding.

## Why it cannot pay off here

**1. The ceiling is small.** From the csynth loop breakdown, on a 4095-byte block
the M-coder spends ~4,095 cycles in Load and ~1,835 in Store out of ~28,121 total —
**I/O is only ~21%**, and Load alone is ~14%. Even perfect overlap of Load bounds
the win at ~1.16×.

**2. The overlap is mostly impossible anyway.** Lane `c` reads the contiguous chunk
starting at `in[c*chunk]`, so the *last* lane needs the *last* bytes of the block.
But the interleaved loop visits lanes round-robin from its very first iteration:

```c
unsigned c = g * CONTEXTS_PER_GROUP + slot;      // iteration 0, GROUPS=4
...
current_byte[c] = input_bank[c][bin_index[c] >> 3];
```

At `GROUPS=4, LANES=16`, iteration 0 already touches lanes 0, 4, 8, **12** — and
lane 12's data lives ~¾ of the way through the input. So Load must be ~75% complete
before encoding can produce anything. The realisable overlap is ~25% of 14% ≈
**3.5%**.

**3. The cross-call form of the same idea is already solved, better.** DATAFLOW's
other benefit is task-level pipelining *across* invocations (load call N+1 while
encoding call N). That requires several kernel calls in flight, which is exactly
what **multiple compute units** give us — and we measured that at **1.95×**
(`../tans/board/onfabric_result.txt`), far beyond DATAFLOW's ~1.03–1.16× here.

## What would actually unlock it: a striped input layout
If lane `c` took bytes `c, c+LANES, c+2·LANES, …` instead of a contiguous chunk,
then after the first `LANES` bytes are loaded *every* lane has data, and Load could
overlap the encode almost completely. The costs are real:
- the compressed container and the host-side decoder both assume contiguous chunks,
  so both would change;
- striding defeats the wide aligned bursts that gave the tANS kernel its **5.4×**
  (blocker B6/B9) — the earlier 64-bit gather experiment on the arith coder lost
  **33%** for exactly this reason.

So the honest conclusion: **contiguous chunking buys burst-friendly I/O and costs
pipelining overlap; striping trades one for the other.** With I/O at 21% and bursts
worth 5.4× elsewhere, contiguous is the right choice for this design — which is why
this experiment was reasoned out rather than built.
