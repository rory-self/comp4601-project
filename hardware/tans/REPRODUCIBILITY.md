# Reproducing every number we publish for `tans`

Each claim below names the **file to use**, the **command to run**, **where to run
it**, and the **output to expect**.

Where to run: 🖥 **workstation** (needs Vitis 2025.2 + KV260 platform) ·
🔌 **board** (KV260 over ssh).

## Setup (once, workstation)

```sh
cd <repo root> && source env.sh
```

> **This design is scope-limited by construction.** The table in
> `src/tans_table.h` is baked from one frequency distribution. tANS is lossless on
> *any* input but only *compresses* data matching that table — it expands an
> aarch64 ELF to 143% and a PDF to 124%. Every number below is on `data/file0..3.bin`,
> the workload class the design targets. Do not quote its ratio against the other
> designs' inputs; see [§7](#7-why-this-design-cannot-share-the-others-input).

---

## 1. CPU speed — **6.67 ns/sym, 8.6× the adaptive coder**

🖥 workstation (this one is an x86 comparison of two algorithms, not a board claim):

```sh
cd hardware/tans/test
g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 \
    tans_bench.cpp ../../replication_full/src/arith5.cpp \
    -I../src -I../../replication_full/src -o tans_bench
./tans_bench <images...>
```

Expect:

| coder | ns/sym |
|---|---:|
| **tANS (table traversal)** | **6.67** |
| arith5 (adaptive) | 57.3 |

Artifact: [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md), which also
carries the compression cost of a static model: **−0.36%** on stationary data
(ties the adaptive coder) up to **+40.7%** on a natural photo.

---

## 2. C-synthesis + co-simulation — **16,892 cycles / 16 KB**

🖥 workstation:

```sh
cd hardware/tans/synth
v++ -c --mode hls --config hls.cfg --work_dir work
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
```

Expect II=2, Fmax ~225 MHz, 16% LUT, **0 DSP**, 16,892 cycles for 16 KB.

The II is 2, not 1, because the `STATE` BRAM read sits inside the state recurrence
— one lookup per byte still beats the M-coder's 8 bins per byte.

The single-lane (`LANES=1`) variant, for the area comparison in
[`../README.md`](../README.md): 16,851 cycles, 5,438 LUT (4%), 0 DSP, **48.6 M
sym/s** kernel-only — ~14× the throughput per LUT of the M-coder.

### Two variants we rejected (both reproducible)

- **DATAFLOW**: 25,461 cycles vs the plain 16,851 — **50% slower**. tANS encodes in
  reverse, so the coder needs the whole block before it can start; Load cannot
  overlap the encoder and the stream handshaking only adds cost.
- **K-way replication on non-stationary data**: K=4 synthesises at 13% LUT and
  multiplies throughput, but each ~1 KB sub-chunk restarts the ANS state under a
  static table, so on a real image it barely compresses (~98%).

---

## 3. Bitstream

🖥 workstation:

```sh
cd hardware/tans/synth
vitis-run --mode hls --package --config hls.cfg --work_dir work
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.xclbin work/arith_kernel.xo
# 2 compute units:
v++ --link --target hw --platform "$PLATFORM" --config link_multi.cfg \
    -o ../bin/arith_multi.xclbin work/arith_kernel.xo
```

`link_multi.cfg` uses `nk=arith_kernel:2` with each CU on its **own HP port pair**
(CU1 → HP0/HP1, CU2 → HP2/HP3) so they do not contend for AXI bandwidth. Both CUs
live in one bitstream, so running 1 vs 2 isolates the effect.

---

## 4. On fabric, 1 compute unit — **147.87 M sym/s, 2.63×**

🔌 board:

```sh
cd <repo root> && ./run_on_board.sh tans
```

Or by hand:

```sh
scp hardware/tans/bin/demo_host_arm hardware/tans/data/file*.bin "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -n 200'
```

Expect:

```
file0: 32768 B  LOSSLESS  ratio 84.00%   ARM 56.11 M sym/s | FPGA 149.61 M sym/s  2.67x
file1: 32768 B  LOSSLESS  ratio 84.15%   ARM 56.26 M sym/s | FPGA 147.14 M sym/s  2.62x
file2: 32768 B  LOSSLESS  ratio 84.06%   ARM 56.32 M sym/s | FPGA 147.05 M sym/s  2.61x
file3: 32768 B  LOSSLESS  ratio 84.12%   ARM 56.32 M sym/s | FPGA 147.71 M sym/s  2.62x

TOTAL 131072 B -> 110206 B (84.08%)
ARM      : 2330.2 us  =  56.25 M sym/s
FPGA     :  886.4 us  = 147.87 M sym/s
SPEEDUP  : 2.63x
```

Artifact: [`results/onfabric_result.txt`](results/onfabric_result.txt).

> **The ARM baseline here is 56.25 M sym/s, ~16× the other designs' 3.47.** That is
> because tANS is byte-wise and they are bit-wise. **Never compare the 2.63× with
> the M-coder's 9.53×** — different denominators. Compare throughput (147.87 vs
> 33.07 M sym/s), not ratios.
>
> The baseline is also *not* the `tans.h` reference coder, which does a
> `vector::push_back` per bit and would inflate the speedup ~3× (18.2 M/s instead of
> the true 56.3). It is the same efficient byte-packing encoder the kernel implements.

---

## 5. Two compute units — **286.65 M sym/s, 1.95× scaling**

🔌 board:

```sh
cd <repo root> && ./run_on_board.sh multi
```

Expect:

```
1 CU  : 111.6 us/call   146.75 M sym/s   lossless=YES
2 CUs : 114.3 us/pair   286.65 M sym/s   lossless=YES
scaling: 1.95x  (2.00x = perfect)
vs the ARM software baseline (56.25 M sym/s):  2.61x -> 5.10x
```

Near-linear, so neither DDR bandwidth nor XRT dispatch limits at this rate. **BRAM
caps this at 2 CUs** (34% per CU, from the per-lane table copies) — a smaller table
or fewer lanes per CU would allow more.

---

## 6. Energy — **23.3 vs 61.6 nJ/byte, 2.64× less**

This is the one number in the project that **cannot** come from a synthesis report.
Vitis reports resources and timing only; Vivado's `report_power` is a
switching-model estimate. This is read from the SOM's INA260 sensor while the
workload actually runs.

🔌 board. Run one engine flat out for 12 s and sample power ~5×/second:

```sh
ssh "$BOARD" 'cd /tmp && XILINX_XRT=/usr ./demo_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -m sw -t 12' &
ssh "$BOARD" 'while :; do cat /sys/class/hwmon/hwmon0/power1_input; sleep 0.2; done'
# repeat with -m hw
```

Sensor: `/sys/class/hwmon/hwmon0/power1_input` (microwatts;
`cat /sys/class/hwmon/hwmon0/name` → `ina260_u14`).

| state | board power | throughput | energy/byte |
|---|---:|---:|---:|
| idle | 3.192 W | — | — |
| ARM software | 3.349 W | 54.3 MB/s | **61.6 nJ/B** |
| FPGA kernel | 3.422 W | 146.7 MB/s | **23.3 nJ/B** |

Artifact: [`results/energy_measurement.txt`](results/energy_measurement.txt).

> The FPGA draws **more** instantaneous power (+0.073 W) yet uses **less** energy
> per byte, because it finishes 2.70× sooner — race to idle. Reporting watts alone
> would have made the accelerator look worse; energy per byte is the honest metric.
>
> Subtracting the 3.192 W idle floor (the marginal cost of doing the work):
> 2.89 nJ/B software vs 1.57 nJ/B hardware = **1.84×**. Quote whichever you like,
> but say which.

---

## 7. Why this design cannot share the others' input

The other three designs are benchmarked on `'a'+(i%7)` or a `.pgm` image. tANS
cannot use either without defeating its own premise: its table is precomputed from
the `file0..3.bin` distribution, and the whole point is that the table is built
**once** and amortised across files that share it.

Feed it the other designs' data and it still round-trips losslessly but barely
compresses — which measures the wrong thing. So:

- **Throughput** is comparable across designs (it is bytes/second either way).
- **Compression ratio is not.** 84.08% here and 41.20% for the M-coder are
  different files.

If a common-corpus comparison is needed, run every design over one corpus **and**
report tANS on its own class separately.

---

## 8. How this design got fast — **it was I/O all along**

Reproducible by reverting the corresponding change; recorded in
[`results/onfabric_result.txt`](results/onfabric_result.txt):

| version | throughput | blocker removed |
|---|---:|---|
| K-way, shared table ROM | 28.2 M/s | — (lanes serialised: one ROM read port) |
| + SIMD lockstep, per-lane tables | 35.8 M/s | lanes actually parallel (II=2) |
| + **64-bit aligned AXI** | **194.0 M/s** | **I/O was 90% of runtime** |
| on real fabric | 147.9 M/s | minus ~24 µs/call XRT overhead |

**No algorithm changed across any of those rows.** Every gain came from removing a
hardware blocker.

The third row is the headline: **widening the AXI port bought 5.4×** (35.8 → 194),
the single largest win anywhere in this project — bigger than the 1.95× from a
second compute unit in §5, and bigger than anything we got from the coder itself.

### Why it worked here and failed on `interleaved`

The identical change made [`../interleaved`](../interleaved) **32.9% slower**
(39,623 → 52,635 cycles). The difference is the access pattern:

| | tANS | interleaved |
|---|---|---|
| reads | one **aligned** 64-bit word after another | K contiguous chunks |
| at a chunk boundary | nothing special | must gather an **unaligned** word |
| result of widening | **+5.4×** | **−32.9%** |

**Wide AXI is not a general win.** It pays when access is aligned and sequential,
and costs when the design must realign at every chunk boundary. Two measurements of
the same optimisation on the same platform, opposite signs — this is the clearest
result we have on the subject, and it generalises: fix the access pattern before
widening the port.

### Why DATAFLOW/streaming does not help either

Measured, not assumed: **25,461 cycles with DATAFLOW vs 16,851 without — 50%
slower.** tANS encodes **in reverse**, so the coder cannot start until the whole
block has arrived. Load can never overlap the encoder, and the stream handshaking
is pure added cost.

That is a structural property of ANS, not a tuning failure. Any streaming version of
this coder would need forward decode — which is exactly the unbuilt design point
(M-coder's forward decode + tANS's byte-wise steps) named in our future work.

### The remaining gap: XRT, not the kernel

Cosim predicts 194 M/s; fabric gives 147.9. At 16 KB blocks the ~24 µs/call XRT
overhead accounts for it. Confirm by varying block size — the gap shrinks as blocks
grow, because the fixed cost amortises. This is the same effect measured on
[`../interleaved`](../interleaved) (62 µs) and
[`../replication_full`](../replication_full) (55 µs); tANS's is smaller because its
blocks are 4× larger.

**Design decision:** widen the port, do not stream. And keep blocks large — at
4 KB the XRT cost would be a third of the call.
