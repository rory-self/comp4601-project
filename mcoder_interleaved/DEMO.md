# Demo runbook — M-coder interleaved (CABAC core + our C-slow interleaving)

### 1. Show the coding logic
- `hls/mcoder.h` — Bryan's multiply-free CABAC engine: the interval split is a
  **256-byte ROM read** (`mc_rlps4`), not a multiply, and renormalisation is
  closed-form (prefix-AND), so there is no variable-trip loop. `hls/CREDIT.txt`
  records the provenance.
- `hls/arith_mc_interleaved.cpp` — our C-slow harness around that core: LANES=16
  coder states visited round-robin by GROUPS=4 shared pipelines → aggregate II=1.

### 2. Show the key synthesis outcomes
`hls/RESULTS.md`: **II=1, Fmax 230 MHz, 23% LUT, ~0 DSP**, 28,121 cycles/4 KB.
Byte-identical to the replicated M-coder on every test case (interleaving re-times
the arithmetic, it does not change it).

### 3. Transfer to the board
```sh
scp board/arith.xclbin  petalinux@10.42.0.25:/tmp/d_mc.bin
scp board/mc_host_arm   petalinux@10.42.0.25:/tmp/h_mc
ssh -t petalinux@10.42.0.25 '
  sudo cp /tmp/d_mc.bin /lib/firmware/xilinx/arith/arith.bin
  sudo xmutil unloadapp; sudo xmutil loadapp arith'
```

### 4. Run the live software-vs-hardware comparison
```sh
ssh petalinux@10.42.0.25 'cd /tmp; export XILINX_XRT=/usr;
  ./h_mc -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000'
```
Verified output: both lossless, HW 1835 B (44.8%), **ARM 2.76 → FPGA 22.5 M sym/s
= 8.13×**.

**Talking point:** the 8.13× is against a *single-stream M-coder* on the ARM. The
M-coder is bit-wise (8 bins/byte), so its CPU baseline is slow; the tree method's
ratio looks smaller (2.63×) yet it is 6.6× faster in absolute terms. Ratios are
only meaningful next to their baseline — see `../DEMO_CHEATSHEET.md`.

**Honest finding:** for this *cheap-datapath* coder, plain replication beats our
interleaving on fabric (31.1 vs 22.6 M sym/s). Interleaving pays off when the
per-lane datapath is expensive (the arith coder's multiply), not here.
