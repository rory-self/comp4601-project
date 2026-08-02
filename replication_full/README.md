# replication_full — K-way replicated adaptive arithmetic coder

Binary range coder (shift, not divide) over a 255-context adaptive bit-tree,
replicated **K** ways. The interval recurrence makes one stream sequential, so
parallelism comes from coding K independent chunks side by side (`-DKWAY=K`).

**Measured:** II=1, Fmax 274 MHz, 37% LUT, 3% DSP. On the KV260: **13.2 M sym/s**
(75.5 ns/sym), lossless, ratio 41.8%. This was the first design on fabric and the
project's reference point. The ARM baseline for the whole project comes from here:
**3.46 M sym/s** (`test/sw_bench.cpp`, KWAY=1).

## Layout
```
src/     arith5.cpp, arith3.h   the coder (KWAY-parameterised)
         arith_board.cpp        board top: arith_kernel(in,n,out,out_len)
         host.cpp               throughput host
         demo_host.cpp          visual image demo (CPU vs FPGA)
test/    kernel_tb.cpp, board_tb.cpp, sw_bench.cpp
synth/   hls.cfg, board.cfg, link.cfg, sweep.sh (K-sweep)
bin/     arith.bin, arith_host_arm, demo_arm
data/    image.pgm, img_smooth.pgm, img_noise.pgm
results/ synth_results.md, onfabric_result.txt, arm_baseline.txt,
         sweep_results.csv, sweep_big_results.csv, lean_variant_note.md
```

## Build & run
```sh
source ../env.sh

# 1. software round-trip + the CPU baseline
cd test
g++ -O2 -Wno-unknown-pragmas -DKWAY=8 ../src/arith5.cpp kernel_tb.cpp -I../src -o t && ./t
g++ -O3 -march=native -Wno-unknown-pragmas -DKWAY=1 ../src/arith5.cpp sw_bench.cpp -I../src -o sw && ./sw

# 2. synthesis (+ co-sim)
cd ../synth && v++ -c --mode hls --config hls.cfg --work_dir work
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
#    expect II=1, Fmax 273.97 MHz, 37% LUT, 3% DSP
./sweep.sh 1 2 4 8 16          # optional: reproduce the K-sweep

# 3. bitstream
v++ -c --mode hls --config board.cfg --work_dir work_board
vitis-run --mode hls --package --config board.cfg --work_dir work_board
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.bin work_board/arith_kernel.xo

# 4. hosts (cross-compile)
cd ../src
"$CXX_ARM" $ARM_CXXFLAGS -I. host.cpp arith5.cpp -DKWAY=8 -o ../bin/arith_host_arm $ARM_LDFLAGS
"$CXX_ARM" $ARM_CXXFLAGS -I. demo_host.cpp arith5.cpp -DKWAY=8 -o ../bin/demo_arm $ARM_LDFLAGS

# 5. run
cd .. && ../run_on_board.sh rep
```

## Visual demo (both implementations on one screen)
```sh
scp bin/demo_arm data/image.pgm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./demo_arm -i image.pgm \
    -x /lib/firmware/xilinx/arith/arith.bin'
```
Prints an ASCII preview before/after, `lossless: YES (pixel-perfect)`,
`CPU vs FPGA output: identical bytes`, and **ARM 2.99 → FPGA 10.81 MB/s = 3.61×**.
