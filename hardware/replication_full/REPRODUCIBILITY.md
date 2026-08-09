# Reproducing every number we publish for `replication_full`

Each claim below names the **file to use**, the **command to run**, **where to run
it**, and the **output to expect**. Everything here was re-verified on 2026-08-08.

Where to run:
- 🖥 **workstation** — needs Vitis 2025.2 + the KV260 platform. The board has no
  compiler and no Vitis.
- 🔌 **board** — the KV260 over ssh. `run_on_board.sh` does the copy/load/run for you.

## Setup (once, workstation)

```sh
cd <repo root>
source env.sh          # finds Vitis, PLATFORM, SYSROOT, CXX_ARM; prints what it found
```

If `env.sh` reports `NOT FOUND` for anything, export it yourself — e.g.
`VITIS_ROOT=/tools/Xilinx/2025.2/Vitis source env.sh`. Software-only steps (1) work
without any of it.

---

## 1. ARM software baseline — **3.47 M sym/s**

This is the baseline for the *whole project*, so it matters that it is exact.

🖥 Cross-compile, 🔌 run on the board:

```sh
cd hardware/replication_full/test
"$CXX_ARM" $ARM_CXXFLAGS -DKWAY=1 -I../src ../src/arith5.cpp sw_bench.cpp -o /tmp/rep_sw_arm
scp /tmp/rep_sw_arm "$BOARD":/tmp/ && ssh "$BOARD" 'chmod +x /tmp/rep_sw_arm && /tmp/rep_sw_arm'
```

Expect:

```
n=4095 symbols, comp=1531 bytes (37.4%)
SW encode: 1178.53 us/message, 287.80 ns/symbol, 3.47 M symbols/s
```

`KWAY=1` matters — the baseline is a *single* stream. Artifact:
[`results/arm_baseline.txt`](results/arm_baseline.txt).

> Do not run this on your x86 workstation and quote the result. The same code gives
> 41.9 ns/sym there (23.9 M sym/s) — 7× faster than the A53. The published baseline
> is the A53, because that is the CPU the FPGA is actually replacing.

---

## 2. C-synthesis — **II=1, Fmax 273.97 MHz, 37% LUT, 3% DSP**

🖥 workstation:

```sh
cd hardware/replication_full/synth
v++ -c --mode hls --config hls.cfg --work_dir work
```

Expect `INFO: [HLS 200-789] **** Estimated Fmax: 273.97 MHz`, and in
`work/hls/syn/report/arith_encode_csynth.rpt`:

| metric | value |
|---|---|
| II (coding loop) | **1** |
| LUT | 43,573 (**37%**) |
| FF | 24,339 (10%) |
| DSP | **48 (3%)** |
| BRAM | 26 (9%) |

The 48 DSPs are 8 lanes × the `range*prob` interval multiply — the multiply that
[`../mcoder`](../mcoder) removes entirely.

---

## 3. Co-simulation — **PASS, all round-trips lossless**

🖥 workstation, after step 2 (same `--work_dir`):

```sh
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
```

Expect:

```
repetitive   in=2048 comp= 672 ratio= 32.81% round-trip=OK
text         in=2000 comp=1424 ratio= 71.20% round-trip=OK
random       in=2048 comp=2115 ratio=103.27% round-trip=OK
tiny         in=  10 comp=  34 ratio=340.00% round-trip=OK
PASS: all round-trips lossless
*** C/RTL co-simulation finished: PASS ***
```

Max latency 38,075 cycles. The `random` row expanding to 103.27% is correct
behaviour, not a bug — an entropy coder cannot compress incompressible data.

> **This step was broken until 2026-08-08.** `hls.cfg` had `tb.cflags=-DKWAY=8`
> with no `-I../src`, while `test/kernel_tb.cpp` does `#include "arith3.h"` from
> `../src`. `tb.file` does not add an include path, so cosim failed with
> `[HLS 207-974] 'arith3.h' file not found` before it ran. Fixed by appending
> `-I../src`. If you have an older checkout, apply that fix first.

---

## 4. K sweep — the K=16 area numbers

🖥 workstation:

```sh
cd hardware/replication_full/synth && ./sweep.sh 1 2 4 8 16
```

Regenerates [`results/sweep_results.csv`](results/sweep_results.csv):

| K | cyc/sym | LUT | LUT % | BRAM | DSP |
|---|---:|---:|---:|---:|---:|
| 4 | 29.2 | 23,792 | 20% | 14 | 24 |
| 8 | 21.5 | 43,573 | 37% | 26 | 48 |
| 16 | 19.4 | **83,337** | **71%** | 50 | **98** |

The presentation quotes "83,156 LUT" for K=16 — the CSV says **83,337**. Cite the
CSV.

---

## 5. Bitstream

🖥 workstation, ~20–40 min:

```sh
cd hardware/replication_full/synth
v++ -c --mode hls --config board.cfg --work_dir work_board
vitis-run --mode hls --package --config board.cfg --work_dir work_board
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.xclbin work_board/arith_kernel.xo
cp ../bin/arith.xclbin ../bin/arith.bin
```

`v++` refuses `-o ...arith.bin` (`[v++ 82-4223] Output file type of .xclbin is
required`), so link to `.xclbin` and copy. Both are the same container — the file
starts with the magic `xclbin2`. A prebuilt `bin/arith.bin` is committed so you can
skip this.

---

## 6. On fabric — **13.27 M sym/s**, kernel-only

🔌 board, driven from 🖥:

```sh
cd <repo root> && ./run_on_board.sh rep
```

Or by hand, for the kernel-only figure:

```sh
scp hardware/replication_full/bin/bench_host_arm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./bench_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000'
```

Expect:

```
input 4095 symbols -> compressed 1712 bytes (41.8071%)
PASS: board output is lossless (decode == input)
mean per call    : 308.657 us  (4095 symbols)
per-symbol       : 75.3742 ns/sym
throughput       : 13.2671 M symbols/s
```

**Timing boundary: kernel-only** — `chrono` around `enqueue+wait`, excluding
host↔device transfer. This is the number comparable with `mcoder` and
`interleaved`. Artifact: [`results/onfabric_result.txt`](results/onfabric_result.txt).

---

## 7. Image demo — **65536 → 42661 B (65.10%), 3.60×**

🔌 board:

```sh
scp hardware/replication_full/bin/demo_arm hardware/replication_full/data/image.pgm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_arm -i image.pgm \
    -x /lib/firmware/xilinx/arith/arith.bin'
```

Expect:

```
original size      : 65536 bytes
compressed size    : 42661 bytes  (65.0955% of original)
lossless           : YES  (reconstructed == original, pixel-perfect)
CPU vs FPGA output : identical bytes
ARM CPU  compress  : 21872.7 us  (2.99625 MB/s)
FPGA     compress  : 6042.48 us  (10.8459 MB/s)
SPEEDUP (FPGA/CPU) : 3.61981x
```

**Timing boundary: includes host↔device transfer.** Do **not** put this MB/s in the
same table as the step-6 figure.

> The presentation's "65 KB → 40 KB (~39% reduction)" is **not** this design — it is
> [`../mcoder`](../mcoder), which gives 40406 B. This design gives 42661 B (34.9%
> reduction).

Repeat with `img_smooth.pgm` and `img_noise.pgm` for the input-sensitivity table:

| image | compressed | ratio | FPGA |
|---|---:|---:|---:|
| image.pgm | 42661 B | 65.10% | 10.78 MB/s |
| img_smooth.pgm | 46157 B | 70.43% | 10.60 MB/s |
| img_noise.pgm | 66916 B | 102.11% | 9.44 MB/s |

**14% throughput swing from input choice alone**, and noise *expands*. Artifact:
[`results/image_demo.txt`](results/image_demo.txt).

---

## 8. Where the time goes — and why this design needed no streaming work

This design is the **control case** for the whole data-movement story: it is the one
kernel whose `m_axi` bursts cleanly, and it is the reason we know the others'
problems are I/O and not the coder.

Check it yourself — compare cosim against fabric:

| | cycles / time, 4095 B | throughput |
|---|---:|---:|
| co-simulation (step 3) | 38,075 cycles = 190.4 µs | 12.9 M sym/s |
| real fabric (step 6) | 308.7 µs/call | 13.27 M sym/s |

Cosim and fabric agree to **~3%**. That only happens because the kernel compresses a
whole 4 KB buffer per call, so the fixed XRT launch cost amortises to near zero per
symbol.

Reproduce the overhead split with the N-sweep:

```sh
for N in 256 1024 2048 4095; do
  ssh "$BOARD" "cd /tmp && XILINX_XRT=/usr ./bench_host_arm \
      -x /lib/firmware/xilinx/arith/arith.bin -N $N -n 1500"
done
```

A straight-line fit of `per-call = overhead + N × per-symbol` gives
**≈55 µs fixed + 62.2 ns/symbol**. At N=4095 the fixed part is 18% of the call; at
N=256 it is 77%. That is why every throughput figure in this repo is quoted at
N=4095 — quoting a small-N number would be measuring XRT, not the coder.

**Design decision this drove:** nothing. The byte-pointer `m_axi` already infers
bursts here, cosim matches fabric, so there was no streaming/DATAFLOW work to do.
Contrast with [`../interleaved`](../interleaved) (bursts *revert*, costing 1.46×)
and [`../tans`](../tans) (I/O was 90% of runtime). Same platform, same AXI, three
different outcomes — the difference is the access pattern, not the interconnect.

---

## 9. What you cannot reproduce

**The lean max-K variant — theory only, never built.**

We explored a maximum-throughput variant of this same K-way idea: keep adaptive
binary arithmetic coding per lane, but with a **reduced model** — 8 probabilities
(one per bit position) instead of the full 255-node context tree, and state narrowed
to 16 bits. That shrinks a lane from ~4,749 LUT / 6 DSP to ~3,389 LUT / 2 DSP, so
many more lanes fit.

Post-HLS co-simulation at K=31:

| metric | value |
|---|---|
| throughput | 28.20 M sym/s @200 MHz (**8.15× ARM**, theoretical) |
| LUT | **95%** (K=32 = 101%, K=64 = 196% — do not fit) |
| compression | pattern **55.75%** vs the full model's 46.50% (worse) |

It did not earn its own design, for three reasons:

1. **It almost certainly will not route.** 95% LUT before the platform's own logic is
   over the practical placement ceiling; Vitis itself flags it "may fail
   placement/routing". It was never board-validated.
2. **It is dominated.** [`../interleaved`](../interleaved) reaches a similar
   throughput class at **19% LUT** while keeping the **full** 255-context model —
   better compression *and* far less area. Lean max-K gives up compression to fill
   the chip; interleaving gives up neither.
3. **No deployable throughput gain and no area saving** relative to the alternatives.

So it stands only as a **theoretical upper bound** on how much coder fits on this
fabric. Treat every number above as theory — none of it ran on silicon.
