# Resource usage — all three designs (Vitis HLS, xck26-sfvc784-2LV-c)

Board-top kernels (`arith_kernel`), the configurations actually built into the
bitstreams. Regenerate any row with:
`cd <design>/synth && v++ -c --mode hls --config board.cfg --work_dir work`
then read `work/hls/syn/report/csynth.rpt`.

| design | config | BRAM | DSP | FF | LUT | Fmax | on fabric |
|---|---|---:|---:|---:|---:|---:|---:|
| **replication_full** | K=8 | 26 (9%) | **48** (3%) | 25,189 (10%) | 44,941 (38%) | 273.97 MHz | 13.2 M sym/s |
| **interleaved** | LANES=16, GROUPS=4 | 50 (17%) | **6** (~0%) | 9,135 (3%) | 23,251 (19%) | 273.97 MHz | 11.7 M sym/s |
| **tans** | K=4 SIMD + 64-bit AXI | 98 (34%) | **0** | 10,111 (4%) | 19,684 (16%) | 224.74 MHz | 147.9 M sym/s |

Standalone (non-board) kernel, for reference:
`interleaved` LANES=16/GROUPS=4 → BRAM 50 (17%), DSP 6, FF 9,706 (4%), LUT 22,525 (19%), **Fmax 342.47 MHz**.

## Reading the table

**DSP is the clearest evidence for the interleaving argument.** Replication gives
every lane its own multiplier — 8 lanes, 48 DSPs. Interleaving shares one
arithmetic datapath across 4 lanes — 16 lanes, **6 DSPs**. Twice the lanes, an
eighth of the multipliers, and half the LUTs.

**tANS uses no DSPs at all** because there is no multiply — the interval bound is a
table lookup. Its cost moves into BRAM instead (34%, the per-lane table copies),
which is what caps it at 2 compute units.

**The trade in one line:** replication spends LUTs and DSPs, interleaving spends
BRAM to save both, and the table method spends BRAM to remove arithmetic entirely.
