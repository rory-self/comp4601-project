# mcoder — multiplier-free table-driven arithmetic coder (H.264 M-coder)

Same problem as [`replication_full`](../replication_full), same K-way chunking,
but the interval arithmetic is replaced by the **H.264/AVC CABAC "M-coder"**.
Probability becomes a **6-bit state index** instead of a 12-bit number, and the
`range × prob` multiply becomes a **384-byte ROM lookup** — so the design uses
**zero DSPs** and the multiply leaves the critical path entirely.

**Measured:** II=1 on both pipeline stages, 0 timing violations, 36% LUT, **0 DSP**,
post-route WNS **+0.754 ns** at 200 MHz. On the KV260: **31.12 M sym/s**
(32.1 ns/sym), lossless — **9.0× the ARM baseline and 2.34× `replication_full`**,
under identical benchmark conditions. Compression is also **5.1% better**, not worse.

## Layout
```
src/     mcoder.h, mcoder_tables.h   the engine + the H.264 tables (384 B)
         mcoder_enc.cpp              software encoder (hosts, tests, ARM baseline)
         mcoder_dec.cpp              the ONE decoder -- hosts and tests all link this
         mcoder_hls.cpp              the kernel: two dataflow stages, II=1
         arith_board.cpp             board top: arith_kernel(in,n,out,out_len)
         bench_host.cpp              throughput host  (kernel-only timing)
         demo_host.cpp               visual image demo (CPU vs FPGA)
test/    kernel_tb.cpp, board_tb.cpp C-sim / co-sim testbenches
         sw_bench.cpp                ARM software baseline
         compare_v5.cpp, v5_prof.*   side-by-side vs replication_full's coder
synth/   hls.cfg, board.cfg, link.cfg, sweep.sh (K + context-storage sweep)
bin/     arith_kernel.xo, bench_host_arm, demo_arm
data/    image.pgm, text_page.pgm, gen_text_page.py
results/ synth_results.md, onfabric_result.txt, arm_baseline.txt,
         how_it_works.md, mcoder_hls_sweep.csv, mcoder_sweep.csv
```

## Build & run
```sh
source ../../env.sh

# 1. software round-trip, the CPU baseline, and the head-to-head vs replication_full
cd test
g++ -O3 -march=native -Wno-unknown-pragmas -Wno-unused-label -DMC_KWAY=1 \
    -I../src sw_bench.cpp ../src/mcoder_enc.cpp -o sw && ./sw
g++ -O2 -Wno-unknown-pragmas -Wno-unused-label -DMC_PROFILE -DMC_KWAY=8 -DV5_KWAY=8 \
    -I../src compare_v5.cpp v5_prof.cpp ../src/mcoder_enc.cpp ../src/mcoder_dec.cpp \
    -o cmp && ./cmp ../data/image.pgm
#    expect: 23/23 lossless, -5.1% vs replication_full at K=8

# 2. synthesis (+ co-sim)
cd ../synth && v++ -c --mode hls --config hls.cfg --work_dir work
vitis-run --mode hls --cosim --config hls.cfg --work_dir work
#    expect II=1 on Bins and Pack, 0 timing violations, 0 DSP
./sweep.sh                     # optional: K = 1..16 x {ctxreg, ctxram}

# 3. bitstream
v++ -c --mode hls --config board.cfg --work_dir work_board
vitis-run --mode hls --package --config board.cfg --work_dir work_board
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o ../bin/arith.bin work_board/arith_kernel.xo

# 4. hosts (cross-compile)
cd ../src
#    -I../../common brings in overhead.h (setup + DMA accounting)
"$CXX_ARM" $ARM_CXXFLAGS -I. -I../../common -DMC_KWAY=8 bench_host.cpp mcoder_dec.cpp \
    -o ../bin/bench_host_arm $ARM_LDFLAGS
"$CXX_ARM" $ARM_CXXFLAGS -I. -I../../common -DMC_KWAY=8 demo_host.cpp mcoder_enc.cpp mcoder_dec.cpp \
    -o ../bin/demo_arm $ARM_LDFLAGS

# 5. run
cd .. && ../../run_on_board.sh mcoder
```

**`MC_KWAY` must match between the kernel and the hosts.** It is not a build
error if they differ — the host would simply misparse the container header and
report a round-trip failure with no hint why.

## Visual demo
```sh
scp bin/demo_arm data/image.pgm "$BOARD":/tmp/
ssh "$BOARD" 'cd /tmp; XILINX_XRT=/usr ./demo_arm -i image.pgm \
    -x /lib/firmware/xilinx/arith/arith.bin -o image.mcz'
./demo_arm -d image.mcz          # decompress it back, no board needed
```
Prints ASCII previews before/after, `lossless: YES (pixel-perfect)`, and
`CPU vs FPGA output: IDENTICAL bytes` — a stronger claim than "it decoded", since
a kernel with a subtly wrong context update would still decode its own output but
would not match the software model bit-for-bit.

`-o` writes the compressed stream as a real file so `ls` shows 65551 → 40484
bytes, and `-d` reads it back; compression and decompression become separately
checkable steps rather than one printed number.

**Only uncompressed input makes sense.** The coder compresses bytes and does not
know they are pixels, but PNG/JPEG are already entropy-coded and it will *expand*
them: `image.pgm` → 67.4%, `image.png` → **101.5%**.

## What is different from `replication_full`

| | replication_full | mcoder |
|---|---|---|
| interval split | `range × prob >> 12` — **48 DSPs** | ROM lookup — **0 DSPs** |
| renormalisation | data-dependent `while` loop | closed form, no loop |
| model update | `prob ± prob>>5` | state-transition ROM |
| context state | 12-bit probability | 7-bit (6-bit state + MPS) |
| cycles/byte (coder) | 83.7 | **8** |
| on fabric | 13.29 M sym/s | **31.12 M sym/s** |

## Reference

The engine is the H.264/AVC CABAC "M-coder":

> D. Marpe and T. Wiegand, "A highly efficient multiplication-free binary
> arithmetic coder and its application in video coding," *ICIP 2003*,
> Barcelona, pp. 263–266. doi:10.1109/ICIP.2003.1246667

> D. Marpe, H. Schwarz, and T. Wiegand, "Context-based adaptive binary
> arithmetic coding in the H.264/AVC video compression standard," *IEEE TCSVT*,
> vol. 13, no. 7, pp. 620–636, July 2003. doi:10.1109/TCSVT.2003.815173

`src/mcoder_tables.h` is transcribed from the **standard**, not the papers:
ITU-T H.264 §9.3, Table 9-44 (`rangeTabLPS`) and Table 9-45 (state transitions).
See [`../../references/README.md`](../../references/README.md).

`results/how_it_works.md` has the derivations, including the two that are
provable rather than empirical: the MPS renormalisation is always 0 or 1 bits,
and the 8-step renormalisation recurrence has a closed form (one barrel shift
plus a prefix-AND).

**The coder is no longer the bottleneck.** At K=8 it accounts for 4096 of the
26,317 measured cycles — **15.6%**. The other 84% is the byte-wide `m_axi`
`Split`/`Concat` copies, which move the same total bytes regardless of K. That
is why K=16 fits after the LUTRAM change yet runs *slower*, and it is where the
next speedup is.
