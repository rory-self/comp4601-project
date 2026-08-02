# bypass_hybrid — entropy-classified hybrid coder (HLS experiment, no bitstream)

Profile `H(bit | ctx)` for each of the 8 bit-tree levels. A level with `H ≈ 1` is
incompressible **given its context** — the adaptive model cannot beat 1 bit, so
paying the arithmetic recurrence for it is wasted work. Those bins are packed
**raw** (exactly 1 bit each, which is optimal); only modelable levels pay the
recurrence. Throughput is then set by `popcount(ADAPT_MASK)` instead of 8.

```
hybrid.cpp       software encoder + decoder (the reference)
hybrid_hls.cpp   HLS top (hybrid_encode_hls)
hybrid_test.cpp  round-trip + ratio for a given mask
hy_0xFF.cfg      all 8 levels adaptive (baseline)
hy_0x0E.cfg      3 adaptive  (the mask measured for file0.bin)
hy_0x00.cfg      0 adaptive  (the mask measured for random data)
RESULTS.md
```
The mask is **derived from measurement**, not guessed — see `../workload_profile/`.

## 0. Environment
```sh
cd <repo root> && source ./env.sh
```
Software steps need only `g++`.

## 1. Software: ratio + losslessness for a mask
```sh
g++ -O2 -Wno-unknown-pragmas -DADAPT_MASK=0x0E hybrid.cpp hybrid_test.cpp -o t
./t ../replication_full/demo/image.pgm ../tans/demo/file0.bin ../replication_full/demo/img_noise.pgm
```
Try `0xFF`, `0x0E`, `0x00` to reproduce the table in `RESULTS.md`. Dropping bins the
model cannot predict is free and even **improves** the ratio slightly.

## 2. HLS synthesis + co-simulation (the throughput claim)
```sh
for M in 0xFF 0x0E 0x00; do
  v++ -c --mode hls --config hy_$M.cfg --work_dir w_$M
  vitis-run --mode hls --cosim --config hy_$M.cfg --work_dir w_$M
done
grep "Estimated Fmax" hy_*_syn.log
```
Expect cycles to scale with the adaptive-bin count — **95.5 → 47.3 → 6.9 cycles/byte
(1.00× / 2.02× / 13.9×)** — and Fmax to *rise* as recurrence pressure drops
(192.8 → 213.3 → 228.3 MHz).

> **Caveat (stated in RESULTS.md too):** absolute cycles/byte are poor here because
> this reuses the *software* packer, whose variable-trip loops do not pipeline. The
> valid result is the **relative** scaling with `ADAPT_MASK`. A production version
> would drop the classifier into the optimised two-stage M-coder packer.

**No bitstream** — this is a synthesis-level experiment, not a board design.
