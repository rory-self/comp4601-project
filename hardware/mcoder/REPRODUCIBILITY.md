# Reproducing every number we publish for `mcoder`

Each claim below names the **file to use**, the **command to run**, **where to run
it**, and the **output to expect**. The bitstream was rebuilt and every board
figure re-measured on 2026-08-08.

Where to run: 🖥 **workstation** (needs Vitis 2025.2 + KV260 platform) ·
🔌 **board** (KV260 over ssh).

## Setup (once, workstation)

```sh
cd <repo root> && source env.sh
```

> **`MC_KWAY` must match between the kernel and every host.** If they differ you get
> no build error — the host misparses the container header and reports a round-trip
> failure with no hint why. The shipped configuration is `MC_KWAY=8`.

---

## 1. ARM software baseline — **2.78 M sym/s**

🖥 cross-compile, 🔌 run:

```sh
cd hardware/mcoder/test
"$CXX_ARM" $ARM_CXXFLAGS -DMC_KWAY=1 -I../src sw_bench.cpp ../src/mcoder_enc.cpp -o /tmp/mc_sw_arm
scp /tmp/mc_sw_arm "$BOARD":/tmp/ && ssh "$BOARD" 'chmod +x /tmp/mc_sw_arm && /tmp/mc_sw_arm'
```

Expect:

```
M-coder software baseline (MC_KWAY=1)
  n=4095 symbols, comp=1554 bytes (37.9%)
  359.28 ns/symbol   =>   2.78 M symbols/s
```

Artifact: [`results/arm_baseline.txt`](results/arm_baseline.txt) (which records 3.20
M/s from an earlier session; 2.78 is the 2026-08-08 re-measurement).

> **The M-coder is *slower* than `replication_full` in software** (2.78 vs 3.47
> M sym/s). Its advantage is architectural — no multiply, fixed-latency
> renormalisation — not a lower operation count. State that plainly: the 9.5× is a
> hardware result, not an algorithmic speedup that would also help a CPU.

---

## 2. C-synthesis — **II=1, 0 DSP, 36% LUT**

🖥 workstation:

```sh
cd hardware/mcoder/synth && v++ -c --mode hls --config hls.cfg --work_dir work
```

Expect:

```
Bins loop        II = 1, depth 7
Pack loop        II = 1, depth 2
Timing           0 violating rows
BRAM             42 (14%)
DSP              0
FF               13170 (5%)
LUT              43165 (36%)
```

`hls.cfg` sets `clock=6.0ns`, but the platform only exposes fixed 100 / 199.998 /
400 MHz — the kernel runs at 5.000 ns regardless. The 6 ns only steers HLS
scheduling.

---

## 3. Co-simulation

🖥 workstation, after step 2:

```sh
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
```

Expect II=1 on `Bins` and `Pack`, 0 timing violations, 0 DSP, and a lossless
round-trip. `hls.cfg` already carries `-I../src` in `tb.cflags` (unlike
`replication_full` and `interleaved`, which had to be fixed).

---

## 4. K × context-storage sweep

🖥 workstation:

```sh
cd hardware/mcoder/synth && ./sweep.sh
```

Regenerates [`results/mcoder_hls_sweep.csv`](results/mcoder_hls_sweep.csv) — 10
configurations, all closing timing at II=1:

| variant | K | cosim cycles | cyc/byte | LUT | LUT % | DSP |
|---|---|---:|---:|---:|---:|---:|
| ctxreg | 1 | 73578 | 14.34 | 11243 | 9% | 0 |
| ctxreg | 4 | 35292 | 6.88 | 35530 | 30% | 0 |
| ctxreg | 8 | 32694 | 6.37 | 67349 | 57% | 0 |
| ctxreg | 16 | 39505 | 7.70 | 131000 | **111% — does not fit** | 2 |
| ctxram | 1 | 75120 | 14.64 | 8049 | 6% | 0 |
| ctxram | 4 | 36834 | 7.18 | 22754 | 19% | 0 |
| **ctxram** | **8** | **34236** | **6.67** | **41797** | **35%** | **0** |
| ctxram | 16 | 41047 | 8.00 | 79896 | 68% | 2 |

**This is the table on presentation slide 9** — it matches digit for digit. Two
defects in how the slide renders it: the fourth column is labelled `K=8` but is
**`K=16`**, and the `K=2` row is dropped.

`ctxram` at K=8 is shipped: −38% LUT for +4.7% cycles.

The compression comparison (**−5.1% vs `replication_full`** at K=8) comes from a
different sweep, [`results/mcoder_sweep.csv`](results/mcoder_sweep.csv), column
`size_delta_pct` = −5.118:

```sh
cd hardware/mcoder/test
g++ -O2 -Wno-unknown-pragmas -Wno-unused-label -DMC_PROFILE -DMC_KWAY=8 -DV5_KWAY=8 \
    -I../src compare_v5.cpp v5_prof.cpp ../src/mcoder_enc.cpp ../src/mcoder_dec.cpp \
    -o cmp && ./cmp ../data/image.pgm
#  expect: 23/23 lossless, -5.1% vs replication_full at K=8
```

---

## 5. Bitstream — **WNS +0.636 ns, timing met**

🖥 workstation, ~10 min:

```sh
cd hardware/mcoder/synth
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.xclbin ../bin/arith_kernel.xo
cp ../bin/arith.xclbin ../bin/arith.bin
```

Do **not** pass `-o ../bin/arith.bin` — it fails with `[v++ 82-4223] Output file
type of .xclbin is required`. Link to `.xclbin`, then copy; both are the same
`xclbin2` container.

Expect in `synth/bc1/reports/bc1/imp/impl_1_system_wrapper_timing_summary_routed.rpt`:

```
WNS 0.636 ns   TNS 0.000   0 failing endpoints of 71515
WHS 0.011 ns   WPWS 1.000 ns   0 pulse-width violations
```

Artifact: [`results/rebuild_2026-08-08.txt`](results/rebuild_2026-08-08.txt).

> **The bitstream behind the original `onfabric_result.txt` was never committed** —
> so until 2026-08-08 the headline design could not be run from this repo. It has
> been rebuilt from the committed `bin/arith_kernel.xo`; the output is byte-identical
> (1687 B), confirming it is the same kernel. `bin/arith.bin` is now tracked.

---

## 6. On fabric — **33.07 M sym/s**, kernel-only

🔌 board:

```sh
cd <repo root> && ./run_on_board.sh mcoder      # runs the image demo (step 7)
```

For the kernel-only figure — **this is the one to publish**:

```sh
scp hardware/mcoder/bin/bench_host_arm "$BOARD":/tmp/mc_bench
ssh "$BOARD" 'cd /tmp && chmod +x mc_bench && XILINX_XRT=/usr ./mc_bench \
    -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000'
```

Expect:

```
input 4095 symbols -> compressed 1687 bytes (41.1966%)
PASS: board output is lossless (decode == input)
mean per call    : 123.825 us  (4095 symbols)
per-symbol       : 30.2381 ns/sym
throughput       : 33.0709 M symbols/s
implied cyc/byte : 6.04762  at 200 MHz
vs V5 on fabric  : 2.4884x  (V5 measured 13.29 M sym/s)
```

**Timing boundary: kernel-only.** Against the 3.47 M sym/s ARM baseline that is
**9.53×**. Artifacts: [`results/onfabric_result.txt`](results/onfabric_result.txt)
(31.12 M/s, original build) and
[`results/rebuild_2026-08-08.txt`](results/rebuild_2026-08-08.txt) (33.07, rebuild).

> Use the *same host binary as the kernel*. Running `replication_full`'s
> `bench_host_arm` against this bitstream reports `FAIL: round-trip mismatch` — the
> container layout and probability model both differ. The timing is still valid
> (it is host-agnostic), but the correctness check is meaningless.

---

## 7. Image demo — **65536 → 40406 B (61.65%)**

🔌 board:

```sh
scp hardware/mcoder/bin/demo_arm hardware/mcoder/data/image.pgm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_arm -i image.pgm \
    -x /lib/firmware/xilinx/arith/arith.bin'
```

Expect:

```
original size      : 65536 bytes
compressed size    : 40406 bytes  (61.6547% of original, 1.62194x)
lossless           : YES  (reconstructed == original, pixel-perfect)
CPU vs FPGA output : IDENTICAL bytes  (hardware matches the software model)
ARM CPU  compress  : 28926.3 us  (2.26562 M symbols/s)
FPGA     compress  : 2945.91 us  (22.2465 M symbols/s)
SPEEDUP (FPGA/CPU) : 9.81917x
```

**This is the presentation's "65 KB → 40 KB, ~39% reduction"** (38.3% exactly).

`text_page.pgm` is a different image — bilevel scanned text, 65536 → 9059 B
(13.8%). Do not mix the two.

---

## 8. ⚠ The two numbers on the results slide that are wrong

`demo_arm` prints them itself, *with a warning*:

```
FPGA     compress  : 22.2465 M symbols/s        <-- became the slide's 21.16 bar
vs V5 on fabric    : 1.61863x   INDICATIVE ONLY - different workload and timing
                     boundary than V5's 13.29.  Use mc_host
                     for the like-for-like figure (2.34x).
vs ARM baseline    : 6.21724x   (V5's published ARM figure, different workload)
                                                <-- became the slide's 6.09x
```

The chart puts this **image-demo** figure alongside three **kernel-only** figures,
and the 6.09× divides an image-demo numerator by a kernel-only denominator.
Measured consistently:

| boundary | M-coder | ARM | speedup |
|---|---:|---:|---:|
| kernel-only (step 6) | 33.07 | 3.47 | **9.53×** |
| image demo (step 7) | 22.25 | 2.27 | **9.82×** |
| slide (mixed) | 21.16 | 3.470 | 6.09× |

Either row is defensible; mixing them is not. **The slide understates this design.**

---

## 9. Where the time goes — **the coder is only 15.6% of it**

This is the most important measurement in this folder, and the one that decided
where the project went next.

At K=8 a 4095-byte buffer splits into 512-byte chunks, so the coder does
512 × 8 bins = **4096 cycles** at II=1. The whole call takes **26,317** cycles
(derivation in §10). So:

| | cycles | share |
|---|---:|---:|
| the coder itself | 4,096 | **15.6%** |
| byte-wide `m_axi` `Split` / `Concat` copies | ~22,221 | **84.4%** |

**The copies move the same total bytes regardless of K.** That single fact explains
three otherwise-confusing results:

1. **Why the engine got ~10× faster but the system only got 2.49×.** Single-stream
   cycles/byte went 83.7 → 8 (multiply removed, renormalisation closed-form), but
   the 84% that is data movement did not move at all.
2. **Why K=16 fits after the LUTRAM change yet runs *slower*** (8.00 vs 6.67
   cyc/byte in `mcoder_hls_sweep.csv`). More lanes divide the 15.6%, not the 84%,
   while adding control overhead.
3. **Why returns collapse before K=16**: K=1→2 buys 36%, 2→4 buys 24%, 4→8 buys 7%,
   8→16 is *negative*.

Check the diminishing returns directly:

```sh
cd hardware/mcoder/synth && ./sweep.sh
awk -F, 'NR>1 && $1=="ctxram" {print $2, $7}' ../results/mcoder_hls_sweep.csv
#  K  cyc/byte  ->  1:14.64  2:9.44  4:7.18  8:6.67  16:8.00
```

Post-route confirms it independently: **the critical path lands in the `Concat`
output copy loop, not in the coder** (Vivado `impl_1`, see step 5).

**Design decision this drove:** stop optimising the coder. Every remaining win in
this design is in the AXI path — widen it, or overlap it. That is exactly what
[`../tans`](../tans) then did, where a 64-bit port bought **5.4×**. It is also why
the M-coder was never given a wide port: we ran out of time, and it is the single
clearest piece of unfinished work in this repo.

> **Caveat before you copy tANS's fix.** Widening is not automatically a win. The
> same change applied to [`../interleaved`](../interleaved) made it **32.9%
> slower**, because that design must gather *unaligned* words at every chunk
> boundary. The M-coder has the same contiguous-chunk layout as interleaved, so it
> would likely need a striped input layout first.

---

## 10. Where `26,317 cycles` comes from

[`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) says the coder is "4096 of the
measured 26,317 cycles = 15.6%". That number is **derived from the board**, not
from cosim:

- `results/onfabric_result.txt` → 31.12 M sym/s at 200 MHz
- 200e6 ÷ 31.12e6 = 6.427 cycles/byte
- 4095 × 6.427 = **26,318 cycles**
- coder's own share: 512 bytes/lane × 8 bins/byte at II=1 = **4096**

Do not look for 26,317 in `mcoder_hls_sweep.csv` — that column is cosim under an
idealised AXI model and reports 32,694 (ctxreg) / 34,236 (ctxram) at K=8.
