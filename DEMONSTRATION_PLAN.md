# Demonstration Plan — Hardware Accelerated Compression using Arithmetic Coding

**Team:** Rory Self, Bryan Bong, Ujjwal Uberoi, Sadat Kabir
**Slot:** 2:45 pm, 3 August · **Duration:** 10–15 min · **Board:** Kria KV260 (Ethernet, `10.42.0.25`)

Everything below runs from **one script** that deploys, loads the bitstream, runs,
and prints both the timing and the correctness check in the terminal:

```sh
./run_on_board.sh <design>      # or: ./run_on_board.sh all
```

---

## What you will see for every design

Each run prints, in the terminal, for **the same workload**:

1. **Software-only result** — the coder running on the board's ARM Cortex-A53:
   throughput (M symbols/s) and µs/call.
2. **Hardware-accelerated result** — the same algorithm on the FPGA fabric:
   throughput and µs/call, timed with `std::chrono` around only the kernel
   enqueue + wait.
3. **Validation** — the host **decodes the board's own compressed output** and
   compares it byte-for-byte with the input, printing `lossless : YES`, plus the
   compression ratio achieved.

So performance *and* correctness for both SW and HW are visible side by side.

---

## Running order (≈12 min)

| # | Step | Command | What it shows | Time |
|---|---|---|---|---|
| 1 | Baseline: software vs first accelerator | `./run_on_board.sh rep` | The original K=8 replicated coder on fabric: 13.2 M sym/s, lossless | 2 min |
| 2 | The best general-purpose design | `./run_on_board.sh mcoder` | Multiply-free CABAC coder: ARM 2.76 → FPGA 22.5 M sym/s, **8.1×**, lossless | 2 min |
| 3 | Workload-specialised design | `./run_on_board.sh tans` | 4 files sharing a frequency table: ARM 56.3 → FPGA 147.9 M sym/s, **2.63×**, ratio 84.08% | 3 min |
| 4 | Using the whole chip | `./run_on_board.sh multi` | Same kernel, 2 compute units: **146.8 → 286.6 M sym/s, 1.95× scaling** | 2 min |
| 5 | Energy comparison | `./tans_host_arm -m sw -t 12` then `-m hw -t 12`, reading the board's INA260 sensor | FPGA **23.3 nJ/byte** vs ARM **61.6 nJ/byte** = **2.64× less energy** | 2 min |
| 6 | Summary table | `./run_on_board.sh all` (pre-run, shown from scrollback) | All designs side by side | 1 min |

**Fallback if the network drops:** every bitstream and host binary is already
staged in `/tmp` on the board, so steps can be run directly over a serial/SSH
session without re-copying. If a design fails to load, `sudo xmutil unloadapp &&
sudo xmutil loadapp arith` re-loads it.

---

## Speaking roles
- **Step 1–2** — the accelerated coders and how correctness is verified.
- **Step 3** — the workload-specialised (tree/tANS) design and why its baseline differs.
- **Step 4–5** — multi-instance scaling and the energy measurement.
- **Step 6** — the summary table and what it means.

---

## Two things we will point out explicitly

**1. Speedup ratios are not comparable across designs — compare throughput.**
Each design is timed against *its own* software baseline, and those differ ~20×
(the bit-wise M-coder runs at 2.76 M sym/s on the ARM; the byte-wise tANS at
56.3 M sym/s). So the M-coder shows **8.1×** while tANS shows **2.63×**, yet tANS
is **6.6× faster in absolute terms**. A speedup number is meaningless without its
baseline — one of our main findings.

**2. Our software baselines are honest.** Our first tANS host reported 8.1×
because its ARM baseline used a reference encoder doing a `vector::push_back` per
*bit*. Replacing it with the same efficient encoder the kernel runs moved the ARM
from 18.2 → 56.3 M sym/s and the speedup to **2.63×**. We report the corrected
figure.

---

## Correctness beyond the demo workload
`ARBITRARY_FILES.md` records round-trip tests on real files — a 1.4 MB PDF, an
aarch64 ELF binary, markdown, and images. The adaptive coders are lossless and
general on all of them. The tree method is lossless on all of them too, but only
*compresses* data matching its baked frequency table (it expands an ELF binary to
143%), which is the workload-class trade the design is built around.

## If asked to see the source
Design is covered in the presentation; per-design walkthroughs are in each
folder's `DEMO.md`, and the full method (workload classifiers, all ideas tried,
and the blocker taxonomy) is in `EXPLORATION.md`.
