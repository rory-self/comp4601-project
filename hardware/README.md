# hardware — FPGA implementations (Kria KV260)

Three HLS designs for the same problem: accelerating adaptive arithmetic coding.
All were built, linked and **measured on the board**, and every run verifies its own
output by decoding it back and comparing with the input.

| design | idea | on fabric | vs its ARM baseline |
|---|---|---:|---:|
| [`replication_full`](replication_full) | K independent coders side by side | 13.2 M sym/s | 3.6× (image demo) |
| [`interleaved`](interleaved) | one shared datapath, 16 coder states C-slow interleaved → II=1 at 18% LUT | 11.7 M sym/s | 3.37× |
| [`tans`](tans) | static table, one lookup per **byte**; ×2 compute units | 147.9 → **286.6** M sym/s | 2.63× / 5.10× |

> **Compare the throughput column, not the ratios.** Each speedup is against that
> design's *own* software baseline, and those differ ~16× (the bit-wise arith coder
> runs at 3.5 M sym/s on the A53; the byte-wise tANS at 56 M sym/s). A ratio is
> meaningless without its baseline.

Every design has the same layout:
```
src/     the coder, the board top (arith_kernel), and the hosts
           bench_host  HARDWARE ONLY  -- profiles the kernel, verifies lossless
           demo_host   SW vs HW       -- times both, what the demo runs
           multi_host  CU SCALING     -- 1 CU vs 2 CUs (tans only)
test/    testbenches (C simulation + co-simulation)
synth/   hls.cfg (C-synthesis/co-sim), board.cfg (-> .xo), link.cfg (-> .xclbin)
bin/     prebuilt bitstream + cross-compiled aarch64 hosts, so the demo runs
         without a 10-40 min Vivado rebuild
data/    inputs
results/ the measurements
```

## Run any of them
```sh
source ../env.sh              # Vitis, platform, sysroot, ARM cross-compiler
../run_on_board.sh            # list designs
../run_on_board.sh tans       # deploy, load the PL, run, verify
../run_on_board.sh all        # all of them + a comparison table
```
Each design's `README.md` has the full path from source to bitstream to board.

**The board has no compiler** (no g++, no XRT headers), so every host is
cross-compiled on the workstation — that is what `env.sh` sets up.
