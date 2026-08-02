# Demo runbook — tree method (static tANS)

**Workload class this targets:** a set of files that **share one frequency table**.
The table of interval bounds is precomputed once, then each file is coded by
*traversing* it — one table lookup per byte, no arithmetic at run time.

### 1. Show the coding logic
- `software/tans.h` — the coder: `state = STATE[...]`, one transition per **byte**
  (the M-coder does 8 bins per byte). No multiply, no per-symbol model update.
- `hls/arith_tans_wide.cpp` — the board kernel: K=4 lanes in a **SIMD lockstep**
  loop, per-lane table copies, 64-bit AXI. The header comment lists the four
  hardware blockers this shape exists to defeat.

### 2. Show the key synthesis outcomes
`hls/RESULTS.md`: single stream II=2, 251 MHz, **4% LUT, 0 DSP**. Wide SIMD K=4:
**16,892 cycles / 16 KB = 194 M sym/s** in co-simulation.
The blocker chain, same algorithm throughout:
**28.2 → 35.8 (SIMD + per-lane tables) → 194 M/s (64-bit AXI)**.

### 3. Transfer to the board
```sh
scp board/arith.xclbin board/tans_host_arm demo/file*.bin petalinux@10.42.0.25:/tmp/
ssh -t petalinux@10.42.0.25 '
  sudo cp /tmp/arith.xclbin /lib/firmware/xilinx/arith/arith.bin
  sudo xmutil unloadapp; sudo xmutil loadapp arith'
```

### 4. Run the live software-vs-hardware comparison
```sh
ssh petalinux@10.42.0.25 'cd /tmp; export XILINX_XRT=/usr;
  ./tans_host_arm -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -n 200'
```
Verified: 4 files, all **LOSSLESS**, ratio **84.08%** (= the source entropy, so the
static table is optimal for this class), **ARM 56.25 → FPGA 147.9 M sym/s = 2.63×**.

### 5. Energy (optional, ~30 s)
```sh
# sustained load + board INA260 sensor
./tans_host_arm -x ... -m sw -t 12   # then read /sys/class/hwmon/hwmon0/power1_input
./tans_host_arm -x ... -m hw -t 12
```
Measured: **FPGA 23.3 nJ/byte vs ARM 61.6 nJ/byte = 2.64× less energy** — the FPGA
draws +0.073 W more but finishes 2.70× sooner (race-to-idle).

**Baseline honesty note:** our first host reported 8.1× because its ARM baseline
used a reference encoder doing a `vector::push_back` per *bit*. The correct
baseline (the same efficient encoder the kernel runs) is 56.3 M/s → **2.63×**.
