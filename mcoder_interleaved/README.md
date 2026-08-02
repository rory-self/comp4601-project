# mcoder_interleaved — multiply-free CABAC core + our C-slow interleaving

The **M-coder** (H.264/CABAC arithmetic engine) replaces the interval multiply with
a 256-byte ROM read and the model update with two 64-byte ROMs — **zero DSPs**, and
it compresses *better* than our range coder. Here that core runs inside our
interleaving harness. **Board-validated: 22.5 M sym/s, 8.13× its ARM baseline.**

> **Attribution:** `mcoder.h`, `mcoder_tables.h`, `mcoder_enc.cpp`, `mcoder_dec.cpp`
> are **Bryan Bong's** M-coder core (branch `m_coderbb`, `brain_storming/mcoder/`).
> `arith_mc_interleaved.cpp` is our interleaving applied to it. See `hls/CREDIT.txt`.

```
hls/    arith_mc_interleaved.cpp   our interleaved encoder over his core
        mcoder.h / mcoder_tables.h Bryan's engine + the 384-byte ROM tables
        mc_il_g{2,4,8}.cfg         GROUPS sweep;  mc_il_l32g8.cfg = 32 lanes
        RESULTS.md, ../DATAFLOW_FINDING.md
board/  arith_mc_board.cpp (top), host.cpp, hls_board.cfg,
        link.cfg (200 MHz), link_x3.cfg (3 CUs), arith.xclbin
```

## 0. Environment (once per shell, works on any machine)
```sh
cd <repo root>
source ./env.sh          # auto-detects Vitis, the KV260 platform, the sysroot and the ARM compiler
```
It prints what it found and names any variable you must set yourself, e.g.
`VITIS_ROOT=/tools/Xilinx/2025.2 source ./env.sh`. Afterwards `$PLATFORM`,
`$SYSROOT`, `$CXX_ARM`, `$VITIS_INCLUDE`, `$ARM_CXXFLAGS` and `$ARM_LDFLAGS` are set.

**Requirements:** Vitis 2025.2, a KV260 platform `.xpfm`, and a PetaLinux/Yocto
sysroot with XRT headers (for cross-compiling the host). Software-only steps (§1)
need none of these — just `g++`.

## 1. Software round-trip
The test checks two things: our interleaved output is **byte-identical** to the
replicated M-coder (so interleaving only re-times the arithmetic, it does not
change it), and it decodes losslessly.
```sh
cd hls
g++ -O2 -Wno-unknown-pragmas -Wno-unused-label -DMC_KWAY=16 -DLANES=16 -DGROUPS=4 \
    arith_mc_interleaved.cpp mcoder_enc.cpp mcoder_dec.cpp mc_interleaved_test.cpp -I. -o t && ./t
```
Expect every case `IDENTICAL` + `LOSSLESS`, then `PASS`.

## 2. HLS synthesis + co-simulation
```sh
cd hls
v++ -c --mode hls --config mc_il_g4.cfg --work_dir work_mc_g4
vitis-run --mode hls --cosim --config mc_il_g4.cfg --work_dir work_mc_g4
```
Expect **II=1, Fmax 230 MHz, 23% LUT, ~0 DSP**, 28,121 cycles per 4 KB.
GROUPS sweep: g8 → 132 MHz (too tight for 200), g4 → **230 MHz (best)**, g2 → 274 MHz
but 2× the cycles.

## 3. Bitstream
```sh
cd board
v++ -c --mode hls --config hls_board.cfg --work_dir work_hls        # top = arith_kernel
vitis-run --mode hls --package --config hls_board.cfg --work_dir work_hls
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o arith.xclbin work_hls/arith_kernel.xo
```
**3 compute units** (23% LUT / 17% BRAM leaves room for three):
```sh
v++ --link --target hw --platform "$PLATFORM" --config link_x3.cfg \
    -o arith_x3.xclbin work_hls/arith_kernel.xo
```

## 4. Host (cross-compile)
Software reference = a **single-stream M-coder** on the ARM, i.e. the same algorithm,
so the comparison is like-for-like.
```sh
cd board
"$CXX_ARM" -std=c++20 -O3 -march=armv8-a -Wno-unknown-pragmas --sysroot="$SYSROOT" \
   -I"$SYSROOT/usr/include" -I. host.cpp -o mc_host_arm \
   -L"$SYSROOT/usr/lib" -lxrt_coreutil -lpthread -luuid
```

## 5. Run
```sh
../../run_on_board.sh mcoder
```
Expect both lossless, HW 1835 B (44.8%), **ARM 2.76 → FPGA 22.5 M sym/s = 8.13×**.

> **Read the 8.13× carefully.** It is against a *bit-wise* software coder (8 bins per
> byte, 2.76 M sym/s). The tree method shows a smaller ratio (2.63×) yet is 6.6×
> faster in absolute terms, because its baseline is 20× quicker. Compare throughput,
> not ratios.

## Two honest findings from this folder
1. **Interleaving loses here.** Bryan's *replicated* M-coder reaches 31.1 M sym/s on
   fabric vs our interleaved 22.6. Interleaving pays only when the per-lane datapath
   is **expensive** (the arith coder's multiply); the M-coder's datapath is cheap
   table lookups, so plain replication wins — and interleaving additionally
   replicates the context memory and couples chunk count to engine count.
2. **DATAFLOW would not help** — ruled out by structure, not tried and failed. See
   `../DATAFLOW_FINDING.md`.
