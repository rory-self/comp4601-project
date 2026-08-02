# Entropy-classified hybrid coder (Idea 4) — results

**The idea.** Profile `H(bit | ctx)` for each of the 8 bit-tree levels. A level
with `H ≈ 1` is incompressible *given its context*: the adaptive model cannot beat
1 bit/bin, so paying the arithmetic recurrence for it is wasted work. Route those
bins to a **raw packed stream** (exactly 1 bit each — optimal for `H=1`) and keep
the arithmetic engine only for levels the model can actually predict.
Throughput is then set by `popcount(ADAPT_MASK)`, not by 8.

The mask is *derived from the measurement*, not guessed — see
`../workload_profile/` and `../EXPLORATION.md` Part 1.

## Compression (software, lossless round-trip on every case)

| file | measured mask | adaptive bins | ratio | ratio with all-8-adaptive |
|---|---|---:|---:|---:|
| image.pgm (photo) | 0xFF | 8 | 58.4% | 58.4% |
| file0.bin (shared-table) | 0x0E (L1,L2,L3) | 3 | **85.3%** | 85.8% |
| img_noise (random) | 0x00 (none) | 0 | **100.2%** | 101.7% |

**Dropping bins the model cannot predict is free — it slightly *improves* the
ratio.** Modelling an `H≈1` bin costs marginally more than the 1 bit a raw bit
costs (adaptation noise + model overhead), so the classifier removes pure waste.
For `img_noise` the full coder was actively *harmful*: it expanded the data to
101.7%, while the classified coder stores it at 100.2% and is far faster.

## Hardware (Vitis HLS, xck26, 6 ns target, cycle-accurate co-simulation)

| mask | adaptive bins | Fmax | cycles (4095 B) | cycles/byte | speedup |
|---|---:|---:|---:|---:|---:|
| 0xFF | 8 | 192.8 MHz | 391,007 | 95.5 | 1.00× |
| 0x0E | 3 | 213.3 MHz | 193,658 | 47.3 | **2.02×** |
| 0x00 | 0 | 228.3 MHz | 28,168 | 6.9 | **13.9×** |

Two effects, both in the right direction:
1. **Cycles scale with the adaptive bin count**, as the classifier predicts.
2. **Fmax *rises* as adaptive bins are removed** (192.8 → 228.3 MHz) — fewer bins
   in the loop-carried path means less pressure on the recurrence. Adjusting for
   clock, the wins become 2.24× and 16.4×.

**Caveat, stated plainly:** the absolute cycles/byte here are poor (95.5 for the
all-adaptive case) because this experiment reuses the *software* packer, whose
variable-trip `while` loops do not pipeline. Bryan's optimised two-stage M-coder
reaches ~8 cycles/byte on the same engine. The valid result here is the
**relative** scaling with `ADAPT_MASK`, which is what the idea predicts; a
production version would drop this classifier into the optimised packer.

## Honest characterisation
This technique accelerates **exactly the files that compress worst**. That is not
a flaw — it is the correct behaviour, and it is useful: it means a system can
detect incompressible data and stop paying for compression it will not get
(`img_noise`: 13.9× faster *and* a better ratio than trying to model it). It is
the entropy-coding analogue of a "stored block" in deflate/zstd, but derived from
a per-bit-level conditional-entropy measurement rather than a whole-block guess.

## Reproduce
```sh
# software: ratio + losslessness for a given mask
g++ -O2 -Wno-unknown-pragmas -DADAPT_MASK=0x0E hybrid.cpp hybrid_test.cpp -o t
./t ../replication_full/demo/image.pgm ../tans/demo/file0.bin ../replication_full/demo/img_noise.pgm
# hardware: synth + cosim for a mask
v++ -c --mode hls --config hy_0x0E.cfg --work_dir w_0x0E
vitis-run --mode hls --cosim --config hy_0x0E.cfg --work_dir w_0x0E
```
