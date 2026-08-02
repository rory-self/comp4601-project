# Energy per byte — measured on the board, all designs

Not from a synthesis report: Vitis reports resources and timing only, and Vivado's
`report_power` is a switching-model estimate. These come from the KV260 SOM's
**INA260 sensor** (`/sys/class/hwmon/hwmon0/power1_input`, total board power in µW)
sampled ~5×/second while each workload runs flat out.

`energy per byte = board power ÷ throughput`

| workload | throughput | board power | **energy / byte** |
|---|---:|---:|---:|
| ARM software — arith coder | 3.46 M sym/s | ~3.36 W* | **~971 nJ/B** |
| FPGA — interleaved | 11.61 M sym/s | 3.362 W | **290 nJ/B** |
| FPGA — replication K=8 | 13.23 M sym/s | 3.318 W | **251 nJ/B** |
| ARM software — tANS | 54.35 M sym/s | 3.361 W | **61.8 nJ/B** |
| FPGA — tANS, 1 CU | 145.7 M sym/s | 3.443 W | **23.6 nJ/B** |
| **FPGA — tANS, 2 CUs** | **286.6 M sym/s** | 3.634 W | **12.7 nJ/B** |

\* the arith software baseline's power was not measured directly; a saturated A53
core draws ≈3.36 W total (measured for the tANS software run), so this row is an
estimate. Every other row is measured.

Idle floor: **3.22 – 3.29 W** depending on session.

## What the numbers say

**1. Energy per byte is set by throughput, not by power.** Board power varies only
3.32 → 3.63 W across everything here (~9%), while throughput varies 3.5 → 287
M sym/s (82×). So the fastest design is essentially always the most energy-efficient
one — "race to idle" dominates.

**2. Multi-instance is also the best energy result.** Two compute units nearly
halve energy per byte (23.6 → 12.7 nJ/B) because throughput doubles for ~5% more
power. Parallelism at constant clock is close to free, energetically.

**3. Interleaving costs energy versus replication** (290 vs 251 nJ/B). It is both
slightly slower on this board *and* draws slightly more power — it trades DSPs for
BRAM (50 vs 26 blocks), and BRAM accessed every cycle is not cheap. The area win
does not carry over into an energy win.

**4. Hardware beats software in every pair**, but by very different margins:
~3.9× for the arith coder (971 → 251) and 2.6× for tANS (61.8 → 23.6), rising to
4.9× with two compute units.

## Caveat
The idle floor drifts ~70 mW between sessions, and the smallest incremental signal
here (replication, ~32 mW above idle) is close to that. The **total-power** figures
above are robust; incremental-power figures should be treated as indicative only.

## Reproduce
```sh
# sustained single-engine load, sampling the sensor during the run
./demo_host_arm -x <bitstream> -d /tmp -m sw -t 14     # software only
./demo_host_arm -x <bitstream> -d /tmp -m hw -t 14     # hardware only
./bench_host_arm -x <bitstream> -N 4095 -n 40000       # hardware only (arith designs)
cat /sys/class/hwmon/hwmon0/power1_input               # µW, sample during
```
