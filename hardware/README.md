# hardware — FPGA implementations (Kria KV260)

Three HLS designs for the same problem: accelerating adaptive arithmetic coding.
All were built, linked and **measured on the board**, and every run verifies its own
output by decoding it back and comparing with the input.

| design | idea | on fabric | vs its ARM baseline |
|---|---|---:|---:|
| [`replication_full`](replication_full) | K independent coders side by side | 13.2 M sym/s | 3.6× (image demo) |
| [`mcoder`](mcoder) | same K-way structure, but the `range × prob` multiply becomes a 384-byte ROM lookup → **0 DSP**, II=1 | **31.1 M sym/s** | **9.0×** (2.34× `replication_full`) |
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
plus one shared file, [`common/overhead.h`](common/overhead.h), included by every
host (`-I../../common`).

## Overhead: the throughput column is not the whole story

The table above is measured with `chrono` around the kernel call. That is the
right boundary for judging a datapath and the wrong one for judging whether
offloading was worth doing, because it excludes everything the CPU never has to
do: opening the device, pushing a ~7.8 MB bitstream into the PL, allocating and
mapping DMA buffers, and copying the payload across the bus and back.

Every host now reports **the same workload at three boundaries**:

| boundary | includes | answers |
|---|---|---|
| 1. compute only | the kernel call | how fast is the datapath? *(the table above)* |
| 2. + host↔device DMA | + memcpy, both `sync`s | what does one offloaded call really cost? |
| 3. + one-time setup | + device open, xclbin load, `xrt::bo` alloc | what does a user running this once experience? |

and the **break-even payload**: setup does not scale with the data, so the FPGA
overtakes the ARM only once the per-byte saving has repaid the fixed cost.
Below break-even the CPU finishes first; above it the gap widens without limit.
Boundary 1 flatters the accelerator, boundary 3 flatters the CPU, and break-even
is the number that actually decides between them.

`demo_host` (and `demo_arm`) prints boundaries 1–3 plus break-even, since it has
a software baseline to compare against. `bench_host` and `multi_host` have none,
so they print the breakdown plus an **amortisation curve** — the effective
throughput at 1, 10, 100, … payloads per process, converging on the steady-state
figure once setup is spread thin enough to disappear.

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
