# HLS tree method (static tANS) — hardware results

The tree method on FPGA: a static table baked from a shared histogram, then each
byte coded by **one state transition** (`state = STATE[...]`) — no multiply, no
per-symbol model update. This is the hardware payoff of the "precompute once,
traverse at runtime" idea (`arith_tans_hls.cpp`, FSE-style reverse encode).

## Verified
Round-trips **losslessly** over all 17 blocks of `image.pgm` (encode with the
baked table, backward ANS decode). `tans_hls_test.cpp`.

## Single stream (LANES=1)
| metric | value |
|---|---|
| II (Enc loop) | 2 (the `STATE` BRAM read sits in the state recurrence) |
| Estimated Fmax | 251 MHz |
| LUT | **5,438 (4%)** |
| DSP | **0** |
| BRAM | 6% |
| cosim latency, 4095 B | 16,851 cycles |
| **throughput @200 MHz (kernel)** | **48.6 M sym/s** |

## Why this matters — byte-wise beats bit-wise
The M-coder codes **8 bins/byte** (II=1 → 8 cyc/byte); tANS codes **1 byte per
state transition** (II=2 → 2 cyc/byte). So even a *single* tANS stream does fewer
cycles/byte than Bryan's 8-engine M-coder, at a fraction of the area:

| design | throughput | LUT | DSP | M/s per %LUT |
|---|---:|---:|---:|---:|
| M-coder replicated K=8 (fabric) | 31.1 M/s | ~35% | 0 | 0.9 |
| interleaved M-coder g4 (fabric) | 22.6 M/s | 23% | ~0 | 1.0 |
| **tANS single stream (cosim, kernel)** | **48.6 M/s** | **4%** | **0** | **~12** |

~14× the throughput-per-LUT. The catch is the model: static (see
`../software/RESULTS.md`) — great for the shared-table / stationary scenario,
poor on non-stationary natural images.

## Variants explored (not kept)
- **DATAFLOW (overlap Load/encode/Store): does NOT help here.** Measured 25,461
  cycles vs the plain 16,851. tANS encodes *in reverse*, so the coder needs the
  whole block before it can start — Load cannot overlap the encoder — and the
  stream handshaking only adds cost. The plain single loop is better.
- **K-way replication: faster but compresses badly on non-stationary data.** K=4
  synthesises at 13% LUT and multiplies throughput, but each ~1 KB sub-chunk
  restarts the ANS state under a static table, so on a real image it barely
  compresses (~98%). Replication suits the *M-coder* (adaptive), not static tANS.

## The niche (why this is the right scope)
The tree method only makes sense for **a set of files that share one frequency
table**: precompute the bounds once (amortised), then each file is pure table
traversal — fast and multiply-free. On such (stationary) data it ties the adaptive
coder's ratio and wins big on speed/area. On arbitrary non-stationary images the
static model costs 24–40% (see `../software/RESULTS.md`), so it is deliberately
**not** a general-purpose compressor and **not** board-built (a fabric run would
show high throughput on data it can't compress).

## Status: DONE (cosim)
HLS + cosim complete, lossless (17/17 blocks). Single-stream = 48.6 M/s kernel @
4% LUT, 0 DSP — the byte-wise throughput/area win, verified. Board build
intentionally skipped: the tree method's value is the stationary/shared-table
niche, and its case is made in cosim.

Reproduce:
```sh
# round-trip (software)
g++ -O2 -Wno-unknown-pragmas -I. -I../software tans_hls_test.cpp arith_tans_hls.cpp -o t && ./t <img.pgm>
# synth + cosim
v++ -c --mode hls --config tans_hls.cfg --work_dir work_tans
vitis-run --mode hls --cosim --config tans_hls.cfg --work_dir work_tans
```
