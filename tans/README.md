# tans — static table-driven coder ("tree method", tANS/FSE)

Build the entropy table **once** from a frequency table the input files share, then
code a whole **byte per state transition** (`state = STATE[...]`) — no multiply, no
per-symbol model update. Kernel = 4 SIMD lanes + 64-bit AXI.

**Measured on the KV260:** 147.9 M sym/s vs 56.3 M/s on the ARM (**2.63×**),
lossless, ratio 84.08%. With **2 compute units: 286.6 M sym/s (5.10×)**, 1.95×
scaling. Energy (board INA260 sensor): **23.3 vs 61.6 nJ/byte = 2.64× less**.

## Layout
```
src/     arith_tans.cpp   the kernel (also the board top: arith_kernel)
         tans_table.h     the baked static table
         tans.h           table construction + software coder
         host.cpp         SW-vs-HW host (also -m sw|hw energy modes)
         multi_host.cpp   1 CU vs 2 CUs scaling test
test/    kernel_tb.cpp    co-simulation testbench
         tans_bench.cpp   software round-trip + ratio + CPU speed
synth/   hls.cfg          C-synthesis / co-sim / .xo
         link.cfg         1 compute unit @200 MHz
         link_multi.cfg   2 compute units on separate HP ports
bin/     arith.xclbin, arith_multi.xclbin, tans_host_arm, multi_host_arm
data/    file0..3.bin     four files drawn from ONE shared distribution
results/ hls_results.md, software_results.md, onfabric_result.txt
```

## Build & run
```sh
source ../env.sh                       # Vitis, platform, sysroot, ARM compiler

# 1. software round-trip + CPU speed
cd test && g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 \
    tans_bench.cpp ../../replication_full/src/arith5.cpp \
    -I../src -I../../replication_full/src -o tans_bench && ./tans_bench

# 2. synthesis + cycle-accurate co-simulation
cd ../synth && v++ -c --mode hls --config hls.cfg --work_dir work
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
#    expect II=2, Fmax ~225 MHz, 16% LUT, 0 DSP, 16,892 cycles / 16 KB

# 3. bitstream
vitis-run --mode hls --package --config hls.cfg --work_dir work
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.xclbin work/arith_kernel.xo
#    2 compute units: use link_multi.cfg -> ../bin/arith_multi.xclbin

# 4. host (the board has no compiler -- cross-compile here)
cd ../src && "$CXX_ARM" $ARM_CXXFLAGS -I. host.cpp -o ../bin/tans_host_arm $ARM_LDFLAGS

# 5. run on the board
cd .. && ../run_on_board.sh tans      # or: ../run_on_board.sh multi
```

## Note on the table
Every symbol must get at least one slot (Laplace smoothing). Without it, encoding a
byte that never appeared indexes a table entry that was never built and the coder
**segfaults**. Smoothing costs 0.088% ratio and makes the coder total.

**Scope:** lossless on any input, but it only *compresses* data matching its baked
table — it expands an aarch64 ELF binary to 143% and a PDF to 124%.
