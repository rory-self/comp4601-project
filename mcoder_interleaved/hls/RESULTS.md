# Interleaved M-coder (iteration 3) — results

**The convergence design:** Bryan's multiply-free CABAC M-coder core (`mcoder.h`,
from `m_coderbb`) run through our C-slow time-interleaving harness. It is the best
design in the project on every axis, and — unlike the arith interleaved coder — it
**closes the 200 MHz platform clock with margin**, so it is genuinely board-usable.

## Correctness
`mc_interleaved_test` (g++): the interleaved output is **byte-identical** to
Bryan's replicated `mc_encode` on every case (pattern/random/mixed/tiny/empty),
and decodes losslessly through his `mc_decode`. Interleaving preserves the exact
arithmetic — it only re-times it.

## Synthesis sweep (LANES=16, xck26, cycle-accurate cosim)

| config | II | Fmax | LUT | DSP | usable clock | 4 KB cycles |
|---|---:|---:|---:|---:|---:|---:|
| g8 (dist 2) | 1 | 132 MHz | — | ~0 | 100 | fewer |
| **g4 (dist 4)** | 1 | **230 MHz** | **23%** | **2 (~0%)** | **200** | **28,121** |
| g2 (dist 8) | 1 | 274 MHz | — | ~0 | 200 | ~2× g4 |

**g4 @ 200 MHz = 29.1 M sym/s = 8.4× ARM** (4095 / (28,121 / 200 MHz)). At its
230 MHz Fmax it would be 33.5 M/s, but 200 is the usable platform clock.

## Why interleaving his core helps
His replicated M-coder is stuck at **167 MHz**: each stream's low/range recurrence
has dependence distance 1, so the `mc_code_bin` path (ROM read → subtract → LZC →
barrel shift → prefix-AND) must close in one cycle. Interleaving makes the
recurrence distance = LANES/GROUPS = 4, giving HLS 4 stages to pipeline that path
→ **230 MHz**, +38%. Multiply-free means ~0 DSP and no width-inference wall.

## Where it lands vs everything else (cosim / on the 200-400 MHz platform)

| design | throughput | usable @200 | LUT | DSP | model |
|---|---:|---:|---:|---:|---|
| arith interleaved g4 | 20.7 M/s @200 (Fmax 293, wasted) | yes | 18% | 48 | 255-ctx |
| replicated M-coder (Bryan, K=8) | ~26 M/s @167 | no (167<200) | 35–57% | 0 | CABAC |
| lean K=31 replication | 28.2 M/s @200 | no (95% LUT won't route) | 95% | — | reduced |
| **interleaved M-coder g4** | **29.1 M/s @200** | **yes, w/ margin** | **23%** | **~0** | **CABAC** |

It also **compresses better**: 1835 B (44.8%) on the pattern vs the arith
interleaved's 1904 B (46.5%).

## BOARD-VALIDATED (the fabric record)
Built the bitstream (`board/`, 200 MHz) and ran it on the KV260. **Lossless**
(HW output decodes == input; 1835 B, matches cosim exactly). Block-size sweep:

| N | 256 | 1024 | 2048 | 4095 |
|---|---:|---:|---:|---:|
| M sym/s | 3.76 | 11.0 | 16.8 | **22.55** |

Fit: ~60 µs overhead + **29.6 ns/sym → ~33.8 M/s asymptotic**. The 29.6 ns/sym
*beats* the cosim estimate (34) — its `m_axi` bursts cleanly, so **real fabric
meets cosim**, unlike the arith interleaved (which lost ~1.46× to un-burstable
DDR). On-fabric standings, 4 KB, same method:

| design | engines | on fabric @4KB | vs ARM |
|---|---:|---:|---:|
| arith interleaved g4 | 4 | 11.66 M/s | 3.4× |
| K=8 replicated arith | 8 | 13.21 M/s | 3.8× |
| interleaved M-coder **g4** (4 eng) | 4 | 22.55 M/s | 6.5× |
| **replicated M-coder K=8** (Bryan, V7) | 8 | **31.12 M/s** | **9.0×** |

Raw log: `board/onfabric_result.txt`.

## Honest conclusion: replication wins *for the M-coder*
Bryan's replicated M-coder (K=8) measured **31.12 M/s** on fabric with the **best
compression** (1687 B / 41.2%) — faster than this interleaved g4 (22.55 M/s).
Matching his engine count (**LANES=32, GROUPS=8**) *does* close 230 MHz with 8
engines, but:
- cosim is only **24,260 cyc** (14% under g4's 28,121, not half — 32 lanes bring
  straggler/idle-slot waste; asymptotic ~33.8 M/s), and
- 32 chunks **compress worse**: 2084 B (50.9%) vs Bryan's 1687 B (41.2%), at 41% LUT.

**Why interleaving loses here:** it pays off only when the datapath is *expensive*
to replicate. The arith coder had a per-lane multiply/DSP, so sharing it won big
(interleaved arith = 18% LUT vs replicated 71%). The M-coder's datapath is *cheap*
(table lookups, no DSP), so replication isn't costly — and interleaving instead
**replicates the 256-entry context memory per lane** (more area) and **couples
chunk count to engine count** (more engines → more chunks → worse compression),
which replication avoids.

**Result — the optimal parallelization strategy depends on datapath cost:**

| coder | datapath | best strategy | evidence |
|---|---|---|---|
| arith | multiply (expensive) | **interleave** (ours) | 18% LUT vs 71% replicated, same throughput class |
| M-coder | table (cheap) | **replicate** (Bryan's) | 31.1 M/s best compression; interleaving costs area + compression |

So Bryan's replicated M-coder is the best fabric design; our interleaving is the
best for the arith coder. Both regimes demonstrated on real hardware.

Reproduce:
```sh
# correctness
g++ -O2 -Wno-unknown-pragmas -Wno-unused-label -DMC_KWAY=16 -DLANES=16 -DGROUPS=4 \
  arith_mc_interleaved.cpp mcoder_enc.cpp mcoder_dec.cpp mc_interleaved_test.cpp -I. -o t && ./t
# synth + cosim
v++ -c --mode hls --config mc_il_g4.cfg --work_dir work_mc_g4
vitis-run --mode hls --cosim --config mc_il_g4.cfg --work_dir work_mc_g4
```
