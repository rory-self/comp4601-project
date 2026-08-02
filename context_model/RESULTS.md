# Higher-order / spatial context modelling — and why it fights our parallelism

Every coder in this project models **one byte in isolation** (an order-0 bit-tree:
a byte's own prefix bits are the only context). For images the redundancy is mostly
**spatial** — a pixel resembles its left and above neighbours — and we never
exploited it. This measures what that costs us, and what taking it would cost in
hardware.

## Measured (empirical conditional entropy, `ctx_study.cpp`)

| context model | image.pgm | img_smooth | contexts | ctx memory |
|---|---:|---:|---:|---:|
| **order-0 (what we built)** | 6.734 b/sym → 84.2% | 6.882 → 86.0% | 1 | **0.4 KB** |
| order-1 (previous byte) | 2.306 → 28.8% | 0.903 → 11.3% | 241 | 90 KB |
| spatial: pixel above | 2.188 → 27.4% | 0.885 → 11.1% | 241 | 90 KB |
| spatial: left+above (4-bit each) | 3.200 → 40.0% | 3.659 → 45.7% | 56 | 21 KB |
| **MED predictor (JPEG-LS)** | **1.202 → 15.0%** | 1.203 → 15.0% | 242 | 91 KB |

*(Our adaptive coder actually reaches ~58% on image.pgm — better than the 84.2%
static order-0 entropy — because it tracks **local** statistics. The comparison
that matters is still stark: a MED-context model reaches ~15%.)*

## The finding: compression and parallelism are in direct conflict

This is the interesting part, and it explains why our fastest designs are also our
weakest compressors:

- Our bit-tree needs **255 probability states per context**. Order-0 = one context
  = **0.4 KB**, which is why we can afford **16 lanes** of it (the whole basis of
  the K-way replication and C-slow interleaving that gave us our speed).
- A MED/spatial model needs ~242 contexts × 255 states ≈ **91 KB per lane**. On a
  KV260 (~324 KB BRAM) that is **~28% BRAM for a single lane** — so only ~2-3 lanes
  fit, against 16 today.

So the ~4× compression win would cost roughly **5-8× of our parallelism**. On
throughput alone that is a bad trade; on *bytes of output per second* it is
arguably a good one, since each byte in now costs ~4× fewer bytes out.

**Context memory is the currency.** Every acceleration technique we built spends
area on parallel lanes; a better model spends the same area on context state. That
is a cleaner way to state the project's central tension than "speed vs ratio".

## Status
Measured in software only — **not implemented in HLS**. It is the clearest
direction for future work, and it reframes our results: we optimised the *speed* of
a deliberately weak model, and the honest summary is that a stronger model would
have been the bigger win for a real compressor.

Reproduce: `g++ -O2 ctx_study.cpp -o ctx_study && ./ctx_study <images.pgm>`
