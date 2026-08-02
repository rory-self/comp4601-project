# replication_full — K-way replicated adaptive arithmetic coder

Binary range coder (shift, not divide) with a 255-context adaptive bit-tree,
replicated **K** ways. The recurrence makes one stream sequential, so parallelism
comes from coding K independent chunks side by side. **Board-validated: 13.2 M sym/s.**

```
hls/       arith5.cpp (coder, -DKWAY=K), arith3.h, arith5_test.cpp, SYNTH.md
software/  sw_bench.cpp — the CPU baseline (KWAY=1)
board/     arith_board.cpp (top), host.cpp, hls_board.cfg, link.cfg, arith.bin
demo/      demo_host.cpp + image.pgm — the visual CPU-vs-FPGA image demo
results/   measured K-sweeps
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

## 1. Software: correctness + CPU baseline (no FPGA tools needed)
```sh
cd hls
g++ -O2 -Wno-unknown-pragmas -DKWAY=8 arith5.cpp arith5_test.cpp -o t && ./t   # round-trip
cd ../software
g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 ../hls/arith5.cpp sw_bench.cpp -I../hls -o sw && ./sw
```
Expect `PASS` / `round-trip=OK`. The `KWAY=1` bench is the honest CPU baseline
(**3.46 M sym/s** on the board's A53).

## 2. HLS synthesis (get II, Fmax, resources)
```sh
cd hls
v++ -c --mode hls --config synth_k8.cfg --work_dir work_k8
grep "Estimated Fmax" *.log ; less work_k8/hls/syn/report/csynth.rpt
```
Expect **II=1, Fmax 273.97 MHz, 37% LUT, 3% DSP** (recorded in `hls/SYNTH.md`).
Co-simulation (cycle-accurate): `vitis-run --mode hls --cosim --config hls_config5.cfg --work_dir work_k8`

## 3. Bitstream: HLS → .xo → .xclbin
`--package` belongs to `vitis-run`, **not** `v++` — `v++ -c` alone leaves RTL but no `.xo`.
```sh
cd board
v++ -c --mode hls --config hls_board.cfg --work_dir work_hls          # csynth (top = arith_kernel)
vitis-run --mode hls --package --config hls_board.cfg --work_dir work_hls
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o arith.xclbin work_hls/arith_kernel.xo                           # ~10-40 min
```
`link.cfg` selects the kernel clock: **id 0 = 100 MHz, id 1 = 200 MHz, id 2 = 400 MHz**
(the platform offers only these three).

**Multiple compute units** (uses the idle chip — 37% LUT means 2 copies fit):
```sh
v++ --link --target hw --platform "$PLATFORM" --config link_x2.cfg -o arith_x2.xclbin work_hls/arith_kernel.xo
```

## 4. Host (cross-compile for the board — the board has no compiler)
```sh
cd board
"$CXX_ARM" -std=c++17 -O3 -mcpu=cortex-a53 --sysroot="$SYSROOT" -I"$SYSROOT/usr/include" \
   host.cpp ../hls/arith5.cpp -DKWAY=8 -I../hls -o arith_host_arm \
   -L"$SYSROOT/usr/lib" -lxrt_coreutil -lpthread -luuid
```
> Do **not** `source` the SDK's `environment-setup-*` script — its `--sysroot` points at
> a stale path. Invoke the compiler directly as above.

## 5. Run on the board
```sh
../../run_on_board.sh rep          # deploys, loads the PL, runs, verifies
```
or manually:
```sh
scp board/arith.bin board/arith_host_arm "$BOARD":/tmp/
ssh -t "$BOARD" 'sudo cp /tmp/arith.bin /lib/firmware/xilinx/arith/arith.bin
                             sudo xmutil unloadapp; sudo xmutil loadapp arith'
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./arith_host_arm \
    -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000'
```
Expect: `PASS: board output is lossless`, 41.8% ratio, **75.5 ns/sym = 13.2 M sym/s**.

## 6. Visual image demo (CPU vs FPGA on one screen)
```sh
scp demo/demo_arm demo/image.pgm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./demo_arm -i image.pgm \
    -x /lib/firmware/xilinx/arith/arith.bin'
```
Expect: ASCII preview before/after, `lossless: YES (pixel-perfect)`,
`CPU vs FPGA output: identical bytes`, **ARM 2.99 → FPGA 10.81 MB/s = 3.61×**.
