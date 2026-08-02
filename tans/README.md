# tans — the "tree method": static table-driven coder (tANS / FSE)

Precompute every interval bound **once** into a table, then code a whole **byte per
state transition** — `state = STATE[...]`, no multiply, no per-symbol model update.
For a set of files that **share one frequency table** this is optimal and very fast.
**Board-validated: 147.9 M sym/s (2.63× the ARM); with 2 compute units, 286.6 M sym/s (5.10×).**

```
software/  tans.h          the coder (normalize / build_enc / build_dec / encode)
           tans_bench.cpp  round-trip + ratio + CPU speed vs the adaptive coder
           RESULTS.md      the ratio-vs-speed trade, measured
hls/       arith_tans_hls.cpp   single stream (II=2, 4% LUT, 0 DSP)
           arith_tans_simd.cpp  K lanes in SIMD lockstep
           arith_tans_wide.cpp  + 64-bit aligned AXI   <- the board kernel
           tans_table.h         the BAKED static table (generated, see §1)
           tans_hls.cfg / simd.cfg / wide.cfg / kway.cfg
board/     host.cpp (SW-vs-HW + energy modes), multi_host.cpp (1 CU vs 2 CUs),
           hls_board.cfg, link.cfg, link_multi.cfg (2 CUs on separate HP ports)
demo/      file0..3.bin — four files drawn from ONE shared distribution
```

## 0. Environment (once per shell, works on any machine)
```sh
cd <repo root>
source ./env.sh
```
See any design README §0 for what it detects and how to override.

## 1. The baked table (regenerate if you change the workload)
`hls/tans_table.h` is generated from the distribution the files share. **Every
symbol must get at least one slot** (Laplace smoothing) — otherwise encoding a byte
that never appeared indexes a table entry that was never built, and the coder
**segfaults**. Smoothing costs 0.088% ratio and makes the coder total.
The generator lives in the project history (`gen_demo`); it writes `TANS_STATE`,
`TANS_DNB`, `TANS_DFS` for `TANS_L = 12` (4096 states).

## 2. Software: correctness, ratio, and CPU speed
```sh
cd software
g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 tans_bench.cpp arith5.cpp -I. -o tans_bench
./tans_bench ../../replication_full/demo/image.pgm
```
Shows lossless round-trips, and **tANS is 8.6× faster than the adaptive coder on the
CPU** — but only compresses well on data matching its table (see `software/RESULTS.md`).

## 3. HLS synthesis + co-simulation
```sh
cd hls
# single stream: II=2, Fmax 251 MHz, 4% LUT, 0 DSP
v++ -c --mode hls --config tans_hls.cfg --work_dir work_tans
vitis-run --mode hls --cosim --config tans_hls.cfg --work_dir work_tans
# the board kernel: K=4 SIMD lanes + 64-bit AXI  -> 16,892 cycles / 16 KB = 194 M sym/s
v++ -c --mode hls --config wide.cfg --work_dir work_wide
vitis-run --mode hls --cosim --config wide.cfg --work_dir work_wide
```
The three configs are the optimisation story: `kway.cfg` (lanes serialised on a
shared ROM), `simd.cfg` (lockstep + per-lane tables), `wide.cfg` (+ wide AXI).
**28.2 → 35.8 → 194 M sym/s with no change to the algorithm.**

## 4. Bitstream
```sh
cd board
v++ -c --mode hls --config hls_board.cfg --work_dir work_hls          # top = arith_kernel
vitis-run --mode hls --package --config hls_board.cfg --work_dir work_hls
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o arith.xclbin work_hls/arith_kernel.xo
# TWO compute units, each on its own HP port pair (HP0/1 and HP2/3):
v++ --link --target hw --platform "$PLATFORM" --config link_multi.cfg \
    -o arith_multi.xclbin work_hls/arith_kernel.xo
```
BRAM (34%/CU, from the per-lane table copies) is what caps this at 2 CUs.

## 5. Hosts (cross-compile)
```sh
cd board
"$CXX_ARM" $ARM_CXXFLAGS -I. host.cpp       -o tans_host_arm  $ARM_LDFLAGS
"$CXX_ARM" $ARM_CXXFLAGS -I. multi_host.cpp -o multi_host_arm $ARM_LDFLAGS
```

## 6. Run
```sh
../../run_on_board.sh tans       # 4 shared-table files: ARM 56.3 -> FPGA 147.9 M sym/s = 2.63x
../../run_on_board.sh multi      # 1 CU vs 2 CUs: 146.8 -> 286.6 M sym/s, 1.95x scaling
```

## 7. Energy (uses the board's INA260 sensor, not a tool estimate)
```sh
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./tans_host_arm -x <bitstream> -d /tmp -m sw -t 12'
ssh "$BOARD" 'cat /sys/class/hwmon/hwmon0/power1_input'    # microwatts, sample during the run
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./tans_host_arm -x <bitstream> -d /tmp -m hw -t 12'
```
Measured: **FPGA 23.3 nJ/byte vs ARM 61.6 nJ/byte = 2.64× less energy** (it draws
+0.073 W more but finishes 2.70× sooner — race to idle).

> **Scope:** lossless on *any* input, but it only **compresses** data matching its
> baked table — it expands an ELF binary to 143%. See `../ARBITRARY_FILES.md`.
