# Reproducing every number we publish for `interleaved`

Each claim below names the **file to use**, the **command to run**, **where to run
it**, and the **output to expect**. Re-verified 2026-08-08.

Where to run: 🖥 **workstation** (needs Vitis 2025.2 + KV260 platform) ·
🔌 **board** (KV260 over ssh).

## Setup (once, workstation)

```sh
cd <repo root> && source env.sh
```

`LANES=16` and `GROUPS=4` are the shipped configuration and must match between the
kernel, the testbench and the hosts.

---

## 1. Software round-trip (no board, no Vitis licence needed)

🖥 workstation. `ap_int.h` compiles under plain g++, so this runs the exact RTL
arithmetic:

```sh
cd hardware/interleaved/src
g++ -O2 -Wno-unused-label -Wno-unknown-pragmas -I"$VITIS_INCLUDE" \
    -DLANES=16 -DGROUPS=4 arith_interleaved.cpp ../test/kernel_tb.cpp -I. -o /tmp/t && /tmp/t
```

Expect the pattern case to compress 4095 → **1904 bytes (46.50%)** and every
round-trip to report OK. That 1904 is the same byte count the board produces —
software, cosim and fabric all agree exactly.

---

## 2. C-synthesis — **two Fmax numbers, and they are both real**

This is the one place our own notes contradicted themselves. There are **two tops**:

### 2a. The coder alone — 342.47 MHz

```sh
cd hardware/interleaved/synth
v++ -c --mode hls --config hls.cfg --work_dir work
```

```
INFO: [HLS 200-789] **** Estimated Fmax: 342.47 MHz
    |  Clock |  Target | Estimated| Uncertainty|
    |ap_clk  |  4.00 ns|  2.920 ns|     1.08 ns|
BRAM_18K 50 (17%)   DSP 6   FF 9,706 (4%)   LUT 22,523 (19%)
```

### 2b. The board top — **273.97 MHz ← cite this one**

```sh
v++ -c --mode hls --config board.cfg --work_dir work_board
```

```
INFO: [HLS 200-789] **** Estimated Fmax: 273.97 MHz
    |ap_clk  |  5.00 ns|  3.650 ns|     1.35 ns|
BRAM_18K 50 (17%)   DSP 6   FF 9,135 (3%)   LUT 23,249 (19%)
```

**Cite 273.97 MHz** — `board.cfg` is what gets linked into the bitstream;
`hls.cfg`'s top is a block never built on its own. The "293 MHz" in older notes was
the standalone number from an earlier source revision.

Artifact: [`results/synth_2026-08-08.txt`](results/synth_2026-08-08.txt).

Two findings worth reporting:

- **The AXI wrapper costs ~20% of the clock** (342 → 274 MHz). The critical path in
  the *built* design is the memory interface, not the coder.
- **`replication_full` reports the same 273.97 MHz** at the same 3.650 ns estimated
  path. Two structurally different coders landing on one critical path is strong
  evidence that neither coder is the limiter — the shared AXI infrastructure is.

---

## 3. Co-simulation — **39,623 cycles for 4095 B**

🖥 workstation, after step 2a (same `--work_dir`):

```sh
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
```

Expect:

```
pattern  in=4095 coded=1904 ratio= 46.50% OK
random   in=2048 coded=2159 ratio=105.42% OK
tiny     in=  10 coded=  58 ratio=580.00% OK
PASS
*** C/RTL co-simulation finished: PASS ***
```

Latency min/avg/**max** = 2858 / 24308 / **39,623** cycles.
Throughput at 200 MHz = 4095 ÷ (39623/200e6) = **20.67 M sym/s**.

Artifact: [`results/cosim_2026-08-08.txt`](results/cosim_2026-08-08.txt).

> **This step was broken until 2026-08-08.** `hls.cfg` had
> `tb.cflags=-DLANES=16 -DGROUPS=4` with no `-I../src`, while
> `test/kernel_tb.cpp` does `#include "arith_interleaved.h"` from `../src`.
> `tb.file` does not add an include path, so cosim died with
> `[HLS 207-974] 'arith_interleaved.h' file not found` before it started — meaning
> **every cosim number we published for this design was unreproducible as
> committed.** Fixed by appending `-I../src`.

### The other cosim configurations

[`README.md`](README.md) also reports 1-engine and 8-engine variants. Neither has
its own `.cfg` any more — override on the command line:

```sh
# 8 engines at a relaxed 150 MHz clock
v++ -c --mode hls --config hls.cfg --work_dir work_g8_150 \
    --hls.clock 6.67ns --hls.syn.cflags "-DLANES=16 -DGROUPS=8" \
    --hls.tb.cflags "-DLANES=16 -DGROUPS=8 -I../src"
```

| configuration | cycles, 4095 B | throughput | LUT |
|---|---:|---:|---:|
| 1 engine | 111,961 | 7.315 M/s | 20,863 (17%) |
| **4 engines** (shipped) | **39,623** | **20.67 M/s** | 23,249 (19%) |
| 8 engines @150 MHz | 27,544 | 22.301 M/s | 24,702 (21%) |

---

## 4. On fabric — **11.667 M sym/s**

🔌 board:

```sh
cd <repo root> && ./run_on_board.sh interleaved
```

Or by hand:

```sh
scp hardware/interleaved/bin/bench_host_arm "$BOARD":/tmp/inter_bench
ssh "$BOARD" 'cd /tmp && chmod +x inter_bench && XILINX_XRT=/usr ./inter_bench \
    -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000'
```

Expect:

```
input 4095 symbols -> compressed 1904 bytes (46.4957%)
PASS: board output is lossless (decode == input)
mean per call    : 351.005 us  (4095 symbols)
per-symbol       : 85.7155 ns/sym
throughput       : 11.6665 M symbols/s
```

**Timing boundary: kernel-only.** Comparable with `replication_full` (13.27) and
`mcoder` (33.07). Artifact:
[`results/onfabric_result.txt`](results/onfabric_result.txt).

### The N-sweep and the overhead fit

Repeat with `-N 256 1024 2048 4095` to reproduce the per-call overhead:

| N | g4 interleaved | K=8 replicated |
|---:|---|---|
| 256 | 80.3 µs / 3.19 M/s | 71.2 µs / 3.59 M/s |
| 1024 | 139.8 µs / 7.32 M/s | 121.5 µs / 8.43 M/s |
| 2048 | 212.4 µs / 9.64 M/s | 183.5 µs / 11.16 M/s |
| 4095 | 351.1 µs / 11.66 M/s | 310.1 µs / 13.21 M/s |

Linear fit `per-call = overhead + N × per-symbol`:
**62 µs + 70.5 ns/sym** (g4), **55 µs + 62.2 ns/sym** (replicated). That ~60 µs is
the XRT per-call overhead quoted in the presentation.

---

## 5. ⚠ "Predicted 31 M sym/s" is a **projection**, not a measurement

It is the cosim cycle count scaled to Fmax: `20.679 × (Fmax / 200)`. Which Fmax you
pick changes the answer:

| Fmax used | projected |
|---|---:|
| 273.97 MHz (board top) | 28.3 M/s |
| 293 MHz (old note) | 30.3 M/s ← where "31" came from |
| 342.47 MHz (coder alone) | 35.4 M/s |

**Only two numbers here are directly measured:** 20.679 M/s (cosim, kernel-only, at
200 MHz) and 11.667 M/s (fabric). Cite those; label anything Fmax-scaled as a
projection.

The cosim→fabric gap is explained in
[`results/onfabric_result.txt`](results/onfabric_result.txt): the platform's clocks
are quantised to 100/200/400 MHz so the headroom is thrown away, and this kernel's
`m_axi` does not burst (HLS warns "inferred burst reverted"), costing ~1.46× on real
DDR versus cosim's idealised fixed-latency AXI model.

---

## 6. Where the cosim→fabric gap goes — a decomposition

Cosim says 20.679 M sym/s; the board gives 11.667. Both run at 200 MHz, so there is
no clock term — the factor of 1.77 splits into exactly two parts:

| term | value | where it comes from |
|---|---:|---|
| measured per-call on fabric | **351.0 µs** | step 4 |
| − XRT per-call overhead | **−62 µs** | step 4 linear fit — the *intercept* of the N-sweep |
| = kernel time on real hardware | **289 µs** | |
| cosim kernel time | 39,623 cyc ÷ 200 MHz = **198.1 µs** | step 3 |
| **real DDR vs idealised AXI** | **289 ÷ 198.1 = 1.46×** | |

Be clear about what is independent here. The 62 µs is measured on its own — it is
the intercept of the four-point N-sweep, obtained without reference to cosim. The
**1.46× is then defined as the leftover ratio**, not predicted and then confirmed.
So this is a *decomposition* of the gap, not a validated model. What it establishes
is how the 1.77 divides between host overhead and memory behaviour, and that no
third term is needed to account for it.

The 1.46× is real DDR behaviour that co-simulation cannot show — cosim models AXI
with a fixed 64-cycle latency (`config_interface -m_axi_latency=64`, printed at the
top of every cosim log) and hides non-coalesced access entirely. Confirm the cause
in the synthesis log:

```sh
grep -i "burst" hardware/interleaved/synth/work/hls/syn/report/*.rpt
#  HLS warns: inferred burst reverted
```

**This is the honest answer to "calculated vs actual".** The gap is the host
interface and the memory system, not a wrong kernel.

---

## 7. Streaming and wide AXI — measured, and **rejected** here

The obvious fix for an I/O-bound kernel is a wider port or DATAFLOW overlap. We
measured both. Neither helps *this* design, and the reasons are worth reporting.

### 7a. Explicit 64-bit gather — **32.9% slower**

Packing eight pixels per external word and gathering them on chip:

| four-engine input path | cycles | throughput | LUT | BRAM |
|---|---:|---:|---:|---:|
| byte pointer, inferred bursts (shipped) | 39,623 | 20.67 M/s | 19% | 17% |
| explicit 64-bit gather | 52,635 | 15.56 M/s | 22% | 23% |

It loses because the design codes **K contiguous chunks**, so every chunk boundary
needs an *unaligned* gather. The cost of realigning exceeds what widening saves,
and on-chip storage grows too.

> **The same change gave [`../tans`](../tans) a 5.4× speedup** (35.8 → 194 M/s).
> tANS reads one aligned 64-bit word after another; this design does not. Wide AXI
> is not a general win — it is a win *if your access pattern is aligned and
> sequential*. That contrast is the clearest result we have on the subject.

### 7b. Why DATAFLOW/streaming was not pursued

Measure the serialised work in the shipped design:

- input staging ≈ 4,095 cycles
- header + compressed output staging ≈ 1,936 cycles on the pattern
- total RTL latency = 39,623 cycles

So data movement is **at most ~15% of latency here** — the opposite of
[`../mcoder`](../mcoder), where it is 84%. Perfectly overlapping *all* staging could
not improve this case by more than **~1.18×**, which does not justify a DATAFLOW
restructure. Arithmetic micro-operations still dominate.

**Design decision:** keep the single loop with inferred bursts. A future streaming
version is only worth building alongside a **striped input layout** or **multiple
AXI channels** — simply widening the existing contiguous-chunk layout loses, as 7a
shows.

---

## 8. The 400 MHz attempt — a **negative** result

🖥 workstation. Reproduce the failure:

```sh
cd hardware/interleaved/synth
v++ -c --mode hls --config hls.cfg --work_dir work_g2 \
    --hls.clock 2.5ns --hls.syn.cflags "-DLANES=16 -DGROUPS=2" \
    --hls.tb.cflags "-DLANES=16 -DGROUPS=2 -I../src"
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    --clock.defaultId 2 -o ../bin/arith_400.xclbin work_g2/arith_kernel.xo
```

HLS reports **402.58 MHz**, II=1. The link then fails:

```
ERROR: [VPL 101-2] design did not meet timing - pulse width violation
```

There is no bitstream to keep, which is why none is committed.

**Why it failed, and why that is the interesting part.** HLS's Fmax is an estimate
from its own scheduling model: it accounts for logic delay on the critical path but
not for real placement, routing congestion, clock skew, or pulse-width constraints
on the actual clocking network. A 402.58 MHz estimate against a 400 MHz target is a
**0.6% margin** — far inside the error bar of that estimate. Post-route reality
consumed it.

This is the same conclusion the project keeps reaching from different directions:
**design to the clock you will actually get, not to Fmax.**

- The platform's clocks are quantised to 100/200/400 MHz, so the board top's
  273.97 MHz buys nothing over 200 — the design still runs at 200.
- An Fmax barely above a clock step will not survive place & route.
- Real throughput came from cycle count and from using more of the chip (multiple
  compute units: **1.95×** on [`../tans`](../tans)), never from chasing the clock.
