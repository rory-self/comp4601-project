# interleaved — C-slow time-interleaved arithmetic coder

Instead of K complete coders, **one shared arithmetic pipeline** visits `LANES`
independent coder states round-robin. A lane is revisited only every
`LANES/GROUPS` cycles, so the loop-carried interval recurrence gets that many
cycles of slack — enough for HLS to schedule the coding loop at aggregate **II=1**.
Same throughput class as replication at **18% LUT instead of 71%**.

```
hls/    arith_interleaved.cpp     v1 (uint16/32)
        arith_interleaved_v2.cpp  v2 — exact-width ap_uint + optional REG_MUL  <- use this
        interleaved_v2_*.cfg      synthesis configs (g2/g4/g8, clock variants)
        RESULTS.md                the full sweep
board/  arith_board_v2.cpp (top), host.cpp, demo_host.cpp, hls_g4.cfg, hls_g2.cfg,
        link.cfg (200 MHz), link_400.cfg (400 MHz — FAILS, see 400MHZ_RESULT.md),
        link_x3.cfg (3 compute units), arith.xclbin
```
`GROUPS` = number of physical shared pipelines. **g4 is the sweet spot**
(recurrence distance 4 → Fmax 293 MHz); g8 is too tight, g2 clocks higher but
runs twice the cycles.

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
`ap_int.h` compiles under plain g++, so the software test exercises the *identical*
arithmetic the RTL uses — no divergence between the two.
```sh
cd hls
g++ -O2 -Wno-unused-label -Wno-unknown-pragmas -I"$VITIS_INCLUDE" -DLANES=16 -DGROUPS=4 \
    arith_interleaved_v2.cpp interleaved_test_v2.cpp -o t && ./t
```
Expect `pattern in=4095 coded=1904 ... OK` and `PASS`.

## 2. HLS synthesis + co-simulation
```sh
cd hls
v++ -c --mode hls --config interleaved_v2_g4.cfg --work_dir work_v2_g4
vitis-run --mode hls --cosim --config interleaved_v2_g4.cfg --work_dir work_v2_g4
grep -E "Estimated Fmax|Final II" *.log
```
Expect **II=1, Fmax 293 MHz, 18% LUT**, 39,623 cycles per 4 KB.
Other variants: `interleaved_v2_g8.cfg`, `interleaved_v2_g2_rm400.cfg` (registered
DSP multiply → Fmax 402 MHz), `interleaved_v2_g4_3ns.cfg`.

## 3. Bitstream
```sh
cd board
v++ -c --mode hls --config hls_g4.cfg --work_dir work_hls       # top = arith_kernel
vitis-run --mode hls --package --config hls_g4.cfg --work_dir work_hls
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o arith.xclbin work_hls/arith_kernel.xo
```
- **3 compute units:** swap `--config link_x3.cfg -o arith_x3.xclbin`.
- **400 MHz attempt:** `hls_g2.cfg` + `link_400.cfg`. This **fails place & route**
  (`[VPL 101-2] pulse width violation`) even though HLS estimates 402 MHz — see
  `board/400MHZ_RESULT.md`. Kept as a documented negative result.

## 4. Hosts (cross-compile)
```sh
cd board
# throughput host (HW only)
"$CXX_ARM" -std=c++20 -O2 --sysroot="$SYSROOT" -I"$SYSROOT/usr/include" \
   host.cpp -o arith_host_v2 -L"$SYSROOT/usr/lib" -lxrt_coreutil -lpthread -luuid
# live SW-vs-HW demo host (software reference = the efficient arith5 coder)
"$CXX_ARM" -std=c++20 -O3 -march=armv8-a --sysroot="$SYSROOT" -I"$SYSROOT/usr/include" \
   demo_host.cpp arith5.cpp -DKWAY=1 -o demo_host_arm \
   -L"$SYSROOT/usr/lib" -lxrt_coreutil -lpthread -luuid
```

## 5. Run
```sh
../../run_on_board.sh interleaved
```
Expect both lossless, **ARM 3.47 → FPGA 11.7 M sym/s = 3.35×**.

> **Why the fabric number (11.7) is below co-simulation (31 M/s):** the platform's
> clocks are quantised, so this design's 293 MHz Fmax still runs at 200; and its
> `m_axi` does not burst, costing ~1.46× on real DDR that co-simulation hid. This is
> the project's clearest "calculated vs actual" case — see `hls/RESULTS.md`.
