# Demo cheat-sheet — all four designs (verified cold, 2 Aug)

Board `petalinux@10.42.0.25` (pw `petalinux1`). One bitstream is live at a time;
each step swaps it in. Everything below was dry-run end-to-end and passes.

## One-time: stage everything on the board
```sh
scp replication_full/board/arith.bin       petalinux@10.42.0.25:/tmp/d_rep.bin
scp replication_full/board/arith_host_arm  petalinux@10.42.0.25:/tmp/h_rep
scp interleaved/board/arith.xclbin         petalinux@10.42.0.25:/tmp/d_il.bin
scp interleaved/board/demo_host_arm        petalinux@10.42.0.25:/tmp/h_il
scp mcoder_interleaved/board/arith.xclbin  petalinux@10.42.0.25:/tmp/d_mc.bin
scp mcoder_interleaved/board/mc_host_arm   petalinux@10.42.0.25:/tmp/h_mc
scp tans/board/arith.xclbin                petalinux@10.42.0.25:/tmp/tans.xclbin
scp tans/board/tans_host_arm tans/demo/file*.bin petalinux@10.42.0.25:/tmp/
ssh petalinux@10.42.0.25 'chmod +x /tmp/h_rep /tmp/h_il /tmp/h_mc /tmp/tans_host_arm'
```

## Load-and-run helper (paste once per ssh session)
```sh
S(){ sudo "$@"; }; export XILINX_XRT=/usr; cd /tmp
load(){ S cp $1 /lib/firmware/xilinx/arith/arith.bin; S xmutil unloadapp; S xmutil loadapp arith; }
```

## The four runs (verified numbers)

| # | design | load | run | expect |
|---|---|---|---|---|
| 1 | replication (arith K=8) | `load /tmp/d_rep.bin` | `./h_rep -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000` | lossless, 41.8%, **75.5 ns/sym = 13.2 M/s** |
| 2 | interleaved (arith C-slow) | `load /tmp/d_il.bin` | `./h_il -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000` | lossless, ARM 3.47 → FPGA 11.7 M/s, **3.35×** |
| 3 | M-coder interleaved (CABAC) | `load /tmp/d_mc.bin` | `./h_mc -x /lib/firmware/xilinx/arith/arith.bin -N 4095 -n 2000` | lossless, **22.5 M/s, 8.13×** |
| 4 | tree method (static tANS) | `load /tmp/tans.xclbin` | `./tans_host_arm -x /lib/firmware/xilinx/arith/arith.bin -d /tmp -n 200` | 4 files lossless, 84.08%, **147.9 M/s, 2.63×** |

Energy (already measured, `-m sw` / `-m hw` on the tANS host + INA260 sensor):
**FPGA 23.3 nJ/byte vs ARM 61.6 nJ/byte = 2.64× less energy.**

---

## Anticipated questions (with the honest answers)

**"Design 3 shows 8.13× but design 4 only 2.63× — isn't 3 better?"**
No — **4 is 6.6× faster in absolute terms** (147.9 vs 22.5 M sym/s). The *ratios*
differ because the CPU baselines differ ~20×: the M-coder is bit-wise (8 bins/byte,
2.76 M/s on the A53) while tANS is byte-wise (1 lookup/byte, 54 M/s). A speedup
ratio is only meaningful against a stated baseline — which is exactly why we report
the workload class and the absolute throughput, not one headline number.

**"Why is the interleaved design slower than plain replication?"** (11.7 vs 13.2)
Two measured reasons: the platform only offers fixed 100/200/400 MHz clocks, so the
interleaved design's 293 MHz Fmax is wasted at 200; and its `m_axi` does not burst,
costing ~1.46× on real DDR. Co-simulation hid both — it predicted 31 M/s. That gap
between calculated and actual is spec step 7b, and we can explain every part of it.

**"Is the tree method's speedup real, given tANS is already fast on CPU?"**
Yes, and we caught ourselves over-claiming here. Our first host reported **8.1×**
because its ARM baseline used a reference encoder doing a `vector::push_back` per
*bit*. Replacing it with the same efficient encoder the kernel runs moved ARM from
18.2 → 56.3 M/s and the honest speedup to **2.63×**.

**"What's the actual contribution?"**
That the achievable acceleration is a **function of the workload class**, and the
class is *measurable up front*. We profile `H(bit|ctx)` per bit-tree level, then
pick the technique: shared frequency table → tree/tANS; unmodelable bins → route
them raw (2.02×–13.9× fewer cycles, *and* slightly better compression); high skew →
run-mode (**measured, rejected**: runs are 1.0–2.8 so it would never fire).

**"Where did the speed actually come from?"**
Mostly from removing *hardware* blockers, not from changing the algorithm. The tree
method went **28.2 → 35.8 → 194 M/s** in co-simulation with an identical coder:
(1) HLS won't parallelise unrolled calls → SIMD lockstep loop; (2) a shared table
ROM has one read port → replicate per lane; (3) byte-serial AXI was 90% of runtime
→ 64-bit aligned AXI (5.4×). Full list: `EXPLORATION.md`, blockers B1–B10.

**"Why does the FPGA use less energy if it draws more power?"**
It draws +0.073 W more, but finishes 2.70× sooner — race-to-idle. Measured on the
board's INA260 sensor, not estimated.

---
Full detail: `EXPLORATION.md` (method + all 7 ideas + blockers), `FRAMING.md`
(narrative), and each design's `RESULTS.md` / `onfabric_result.txt`.
