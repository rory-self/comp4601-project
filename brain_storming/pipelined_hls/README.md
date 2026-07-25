# V6 — pipelined single-stream coder (iteration 2)

Iteration 1 (`best_hls/`, V5) got its speedup by **replication**: K complete
coders side by side, each one still sequential (~9 cycles per coded bit).
V6 attacks the *single stream*: the coder is restructured into a **flat state
machine** whose loop body does exactly one small unit of work per iteration —

| state | work per cycle |
|---|---|
| `S_CODE` | one interval update (codes one binary decision) |
| `S_RENORM` | one renormalisation step E1/E2/E3 (emits ≤ 1 bit) |
| `S_DRAIN` | emits one deferred "pending" underflow bit |
| `S_FLUSH` | the final flush decision |

Because every iteration now has the same small fixed shape, the loop can be
pipelined (`#pragma HLS PIPELINE II=1`) — one work unit per clock. V5's
`encode_bit()` could not be pipelined because it hides a *variable-length*
renorm loop inside each coding step (no fixed pipeline shape; a forced
pragma does not converge).

**This is pipelining, not unrolling** — unrolling (the V5 `KWAY` trick) copies
hardware in space across independent streams; pipelining overlaps work in time
*within one stream*. They multiply: V6 keeps the same K-way top level, so each
of the K replicated coders is now also pipelined.

## Status

- **Verified bit-exact with V5** (`arith6_test.cpp` compares outputs
  byte-for-byte at K=1..16), so the V5 decoder, board host and demo work
  unchanged.
- **Verified lossless** (round-trip through the V5 decoder).
- **Not yet synthesised.** Cycle predictions below assume II=1 is achieved.

## Predicted performance (from the host-side work-unit counter)

One flat-loop iteration ≈ one clock at II=1. Measured units/symbol on the
test inputs: ~11.7 (repetitive) to ~21.1 (random), ~17.5 mixed — vs V5's
measured **83.7 cycles/symbol** single stream → **~4–7× per stream**, on top
of replication.

The known risk: the loop-carried recurrence (`low`/`high` → 17×12 multiply →
compare → next state) must fit in one 5 ns cycle. If Vitis reports II=2–3
instead, the gain shrinks proportionally but remains a solid multiplier.
With II=1 the `Split`/`Concat` copy loops become a significant serial
fraction (Amdahl) — overlapping them with encoding via DATAFLOW is the next
optimisation after II is confirmed.

## Build & run

```
# software: correctness + bit-exactness + cycle prediction
g++ -O2 -Wno-unknown-pragmas -DKWAY=8 arith6.cpp arith6_test.cpp -o t && ./t

# Vitis HLS: check the achieved II in the csynth report, then cosim
vitis-run --mode hls --csim  --config hls_config6.cfg   # (add -DKWAY=8 via syn.cflags/tb.cflags if sweeping)
v++ -c --mode hls --config hls_config6.cfg
```
Look for `Flat` in the csynth report: `II = 1` is the goal; the achieved II
directly scales all predictions above.
