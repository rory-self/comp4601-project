# The tree method (static tANS) — precompute once, traverse at runtime

**Scenario it targets:** a set of files/images that share one frequency table.
Precompute the entropy table **once** (amortised across all files), then every
file is coded by **traversing that table** — `state = table[state][symbol]` — with
**no arithmetic and no multiply at runtime**. This is table ANS (tANS / FSE), the
concrete form of "precompute every reachable interval bound, then move around the
tree instead of computing it each symbol."

`tans.h` is a from-scratch, lossless static order-0 tANS coder (~120 lines). It
round-trips losslessly on pattern / random / mixed / tiny inputs and on all test
images.

## The trade, measured

**Speed (the payoff).** Shared table built once, then encode:

| coder | ns/sym | note |
|---|---:|---|
| **tANS (table traversal)** | **6.67** | no arithmetic, no multiply, byte-per-lookup |
| arith5 (adaptive) | 57.3 | a multiply per bin, per-symbol model update |

→ **tANS is 8.6× faster** on the CPU. On FPGA the gap is larger in kind: no DSP,
short critical path, one lookup per byte.

**Compression (the cost) — depends on how stationary the data is:**

| data | tANS + table | adaptive | delta |
|---|---:|---:|---:|
| noise (fixed/stationary distribution) | 66,145 | 66,384 | **−0.36% (ties)** |
| smooth gradient (non-stationary) | 56,719 | 45,794 | +23.9% |
| natural photo (non-stationary) | 55,593 | 39,523 | +40.7% |

The loss is the **static model**, not the table (only ~380 B, and amortised to 0
in this scenario). A single fixed table cannot track the *within-file* variation
that a natural image has; a genuinely shared/stationary distribution has no such
variation, so tANS ties the adaptive coder there.

## When to use it
- **Yes:** many files with the same (roughly stationary) distribution, where
  runtime speed / multiply-free hardware matters more than the last ~few % of
  ratio — e.g. fixed-alphabet telemetry, a known symbol source, repeated
  same-kind frames. Table cost amortises away; you get 8.6× the speed.
- **No:** arbitrary natural images, where within-image non-stationarity makes a
  static model cost 24–40%. There the adaptive path wins (and the multiply-free
  *and* adaptive option is the M-coder — see the M-coder on branch `m_coderbb` (teammate's work)).

## Design map (multiply-free coders)
| coder | multiply-free | adaptive | speed | compression |
|---|:--:|:--:|---|---|
| **tree / tANS (this)** | ✓ | ✗ (static table) | fastest (1 lookup/byte) | best on stationary, poor on non-stationary |
| M-coder (CABAC) | ✓ | ✓ (state ROMs) | fast (1 lookup/bin) | best overall |

Reproduce:
```sh
g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 tans_bench.cpp arith5.cpp -I. -o tans_bench
./tans_bench <images...>
```
