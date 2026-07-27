# KV260 bitstream build — time-interleaved arithmetic coder (iteration 2)

**Goal for the build machine:** produce `arith.xclbin` and send it back. The
long step is the Vivado place-&-route inside `v++ --link` (~30–60 min; a faster
CPU is exactly why this is being handed off). You do **not** need the board.

## Requirements
- **Vitis 2025.2.1** (the `.xclbin` is version- and platform-locked, so the
  version must match the machine that runs it).
- The bundled platform at `platform/kv260_custom/kv260_custom.xpfm` (already in
  this folder — a custom KV260 platform).

## Build

```sh
# Safe / recommended: 4 engines @ 200 MHz  -> ~20.7 M sym/s on fabric
VITIS=/path/to/Xilinx/2025.2.1/Vitis ./build.sh

# "Max clock" attempt: 2 engines + registered DSP mul @ 400 MHz
#   HLS Fmax ~402 MHz — thin margin, may FAIL to close timing post-route.
#   If it closes: ~25.7 M sym/s. If not, v++ errors on timing; fall back to g4.
VITIS=/path/to/Xilinx/2025.2.1/Vitis ./build.sh hls_g2.cfg link_400.cfg
```

Set `VITIS` to your install dir (the folder containing `settings64.sh`). If the
platform is elsewhere, also pass `PLATFORM=/abs/path/kv260_custom.xpfm`.

Output: **`arith.xclbin`** ← send this file back.

## Why two variants (the clock story)
The interval-update recurrence in arithmetic coding is the hard part; we hide it
by C-slow interleaving `LANES=16` independent coder states through a shared
pipeline, so the encode loop runs at **aggregate II=1**. `GROUPS` = number of
physical engines:

| variant | engines | cycles/4 KB | HLS Fmax | usable platform clock | throughput |
|---|---:|---:|---:|---:|---:|
| **g4** (default) | 4 | 39,623 | 293 MHz | 200 MHz | **20.7 M/s** |
| g2 (max attempt) | 2 | 63,854 | 402 MHz | 400 MHz | 25.7 M/s *(if it closes)* |

This platform only offers fixed **100 / 200 / 400 MHz** clocks. g4's 293 MHz
gets rounded **down** to 200; g2 was tuned (registered multiply) specifically to
clear 400. Both are lossless and use the full 255-context adaptive model
(verified by C/RTL co-simulation).

## Optional: verify before the long link
```sh
source $VITIS/settings64.sh
# C simulation (fast, software):
v++ -c --mode hls --config hls_g4.cfg --work_dir work_hls   # then check reports
# or full cycle-accurate C/RTL co-sim:
vitis-run --mode hls --cosim --config hls_g4.cfg --work_dir work_hls
```
Expect: `pattern in=4095 coded=1904 ... OK`, `PASS`.

## Files
- `arith_interleaved_v2.{cpp,h}` — the interleaved encoder core (ap_uint-tight).
- `arith_board_v2.cpp` — board top `arith_kernel(in, n, out, out_len)`.
- `arith_board_v2_test.cpp` — testbench (encode → decode → lossless check).
- `hls_g4.cfg` / `hls_g2.cfg` — HLS configs (kernel + clock target).
- `link.cfg` / `link_400.cfg` — link configs (200 MHz / 400 MHz clock id).
- `build.sh` — HLS → package → link, one shot.
- `host.cpp` — XRT host (runs on the board; decoder set to 16 lanes). Not needed
  to build the bitstream — included so the board side is self-contained.
- `platform/` — the KV260 platform to link against.
