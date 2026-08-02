# interleaved — C-slow time-interleaved arithmetic coder

One shared arithmetic datapath serves `LANES=16` independent coder states
round-robin. A lane is revisited only every `LANES/GROUPS` cycles, so the
loop-carried interval recurrence gets that many cycles of slack — enough for HLS to
schedule the coding loop at aggregate **II=1** *and* raise the clock.
`GROUPS=4` is the shipped configuration (recurrence distance 4).

**Measured:** II=1, Fmax 293 MHz standalone / 274 MHz as the board top, **18% LUT**
(vs 71% for equivalent replication). The design uses 23,251 LUTs, 6 DSPs and 50 BRAMs. On the KV260: **11.7 M sym/s**, ARM 3.47 →
**3.35×**, lossless.

> Why fabric (11.7) is below co-simulation (31 M/s): the platform offers only fixed
> 100/200/400 MHz clocks so the 293 MHz Fmax still runs at 200, and this kernel's
> `m_axi` does not burst, costing ~1.46× on real DDR. Both are measured — see
> `results/`. The mechanism is explained in `results/how_it_works.md`.

## Layout
```
src/     arith_interleaved.cpp/.h  the coder (exact-width ap_uint, optional REG_MUL)
         arith_board.cpp           board top: arith_kernel(in,n,out,out_len)
         bench_host.cpp                  throughput host
         demo_host.cpp             live SW-vs-HW host
test/    kernel_tb.cpp, board_tb.cpp
synth/   hls.cfg    standalone kernel (C-synthesis + co-sim)
         board.cfg  board top -> .xo
         link.cfg   1 compute unit @200 MHz
bin/     arith.xclbin, bench_host_arm, demo_host_arm
results/ hls_results.md, onfabric_result.txt, 400mhz_attempt.md, how_it_works.md
```

## Build & run
```sh
source ../../env.sh

# 1. software round-trip (ap_int.h compiles under g++, so this is the exact RTL maths)
cd src && g++ -O2 -Wno-unused-label -Wno-unknown-pragmas -I"$VITIS_INCLUDE" \
    -DLANES=16 -DGROUPS=4 arith_interleaved.cpp ../test/kernel_tb.cpp -I. -o /tmp/t && /tmp/t

# 2. synthesis + co-simulation
cd ../synth && v++ -c --mode hls --config hls.cfg --work_dir work
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
#    expect II=1, Fmax ~293 MHz, 18% LUT, 39,623 cycles / 4 KB

# 3. bitstream (board top)
v++ -c --mode hls --config board.cfg --work_dir work_board
vitis-run --mode hls --package --config board.cfg --work_dir work_board
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.xclbin work_board/arith_kernel.xo

# 4. hosts (cross-compile; the board has no compiler)
cd ../src
"$CXX_ARM" $ARM_CXXFLAGS -I. bench_host.cpp -o ../bin/bench_host_arm $ARM_LDFLAGS
"$CXX_ARM" $ARM_CXXFLAGS -I. -I../../replication_full/src demo_host.cpp \
    ../../replication_full/src/arith5.cpp -DKWAY=1 -o ../bin/demo_host_arm $ARM_LDFLAGS

# 5. run
cd .. && ../../run_on_board.sh interleaved
```

`results/400mhz_attempt.md` records a negative result: the one variant with an HLS
Fmax above 400 (GROUPS=2 + registered DSP multiply, 402.58 MHz) **failed place &
route** at the 400 MHz clock. Design to the clock you get, not to Fmax.
