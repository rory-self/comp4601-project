# Demo runbook — replication_full (iteration 1, V5)

The four demo steps, with exact commands. Board = `petalinux@10.42.0.25`.

### 1. Show the coding logic
- `hls/arith5.cpp` — binary range coder (shift, not divide) with an adaptive
  255-context bit-tree, replicated **K-way** (`-DKWAY=K`), on `hls/arith3.h`.
- Board wrapper: `board/arith_board.cpp` (`arith_kernel(in, n, out, out_len)`, K=8).

### 2. Show the key synthesis outcomes
- `hls/SYNTH.md` — headline: **II=1, Fmax 273.97 MHz, 37% LUT, 3% DSP** (K=8).
  Regenerate the report: `v++ -c --mode hls --config hls/synth_k8.cfg --work_dir work_k8_report`.
- K-scaling sweep: `results/sweep*.csv` (K=8 → 5.4×, K=16 → 7.0×).

### 3. Transfer to the board
```sh
# from replication_full/
scp board/arith.bin  petalinux@10.42.0.25:/tmp/arith_k8.bin
scp demo/demo_arm    petalinux@10.42.0.25:/tmp/
scp demo/image.pgm   petalinux@10.42.0.25:/tmp/
# load the K=8 bitstream (root; board pw):
ssh -t petalinux@10.42.0.25 '
  sudo cp /tmp/arith_k8.bin /lib/firmware/xilinx/arith/arith.bin
  sudo xmutil unloadapp; sudo xmutil loadapp arith '     # -> "Loaded with slot_handle 0"
```
(`demo_arm` is prebuilt for aarch64; `demo/run_demo.sh <BOARD_IP>` does the whole
deploy+run in one step. Rebuild the bitstream via `board/hls_board.cfg` +
`board/link.cfg`, or the host from `demo/demo_host.cpp`.)

### 4. Run the live software-vs-hardware comparison
```sh
ssh petalinux@10.42.0.25 'cd /tmp; export XILINX_XRT=/usr;
  ./demo_arm -i image.pgm -x /lib/firmware/xilinx/arith/arith.bin'
```
Compresses a real image on **both** the ARM CPU and the FPGA, decompresses it,
checks pixel-perfect losslessness, and prints an ASCII preview + timing. Expected
(256×256 image, measured):
```
lossless         : YES   (reconstructed == original)
ARM CPU compress : ~21,900 us   (2.99 MB/s)
FPGA    compress :  ~6,070 us   (10.79 MB/s)
SPEEDUP          : ~3.6x
```
This is the board-validated iteration-1 result: **K=8 = 3.84× the ARM** on the
throughput host (`board/host.cpp`, raw log `board/onfabric_result.txt`); the image
demo shows a similar ~3.6× end-to-end including decode/verify.
