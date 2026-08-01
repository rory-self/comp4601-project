# Demo runbook — interleaved (iteration 2)

The four demo steps, with exact commands. Board = `petalinux@10.42.0.25`.

### 1. Show the coding logic
- `hls/arith_interleaved_v2.cpp` — the C-slow time-interleaved encoder (one shared
  pipeline rotates through `LANES=16` coder states → aggregate **II=1**).
- Board wrapper: `board/arith_board_v2.cpp` (`arith_kernel(in, n, out, out_len)`).

### 2. Show the key synthesis outcomes
- `hls/RESULTS.md` — the full sweep (g4/g8/g2, Fmax, cycles, LUT).
- Headline: **g4 = II=1, Fmax 293 MHz, 18% LUT**, full 255-context model;
  registered-DSP-mul variant **g2 = 402 MHz**. Logs: `hls/v2_*_synth.log`.

### 3. Transfer to the board
```sh
# from interleaved/board/
scp arith.xclbin   petalinux@10.42.0.25:/tmp/arith_v2.xclbin
scp demo_host_arm  petalinux@10.42.0.25:/tmp/
# load the interleaved bitstream (root; board pw):
ssh -t petalinux@10.42.0.25 '
  sudo cp /tmp/arith_v2.xclbin /lib/firmware/xilinx/arith/arith.bin
  sudo xmutil unloadapp; sudo xmutil loadapp arith '     # -> "Loaded with slot_handle 0"
```
(`demo_host_arm` is prebuilt for aarch64. To rebuild it, see the compile line at
the top of `board/demo_host.cpp`; to rebuild the bitstream, `board/build.sh` +
`board/DEPLOY.md`.)

### 4. Run the live software-vs-hardware comparison
```sh
ssh petalinux@10.42.0.25 'cd /tmp; export XILINX_XRT=/usr;
  ./demo_host_arm -x /tmp/arith_v2.xclbin -N 4095 -n 2000'
```
Software reference = the efficient single-stream CPU coder (arith5) on the ARM;
hardware = the interleaved FPGA kernel. Expected (measured):
```
SW lossless : YES        HW lossless : YES
ARM  software (arith5) : ~1178 us/call   (3.48 M sym/s)
FPGA interleaved kernel:  ~351 us/call   (11.65 M sym/s)
SPEEDUP (HW vs SW)     : 3.35x
```
Talking point: the single-stream SW compresses *better* (37.4%) than the 16-chunk
HW (46.5%) — the throughput-vs-ratio trade, live. And note this is the honest
baseline: the SW number (3.48) equals the project's ARM baseline (3.46).
