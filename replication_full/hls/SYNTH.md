# Key synthesis outcomes — replication (V5), K=8

Vitis HLS 2025.2, target `xck26-sfvc784-2LV-c`, `arith_encode` (K=8), clock 5 ns.
Full report: `work_k8_report/hls/syn/report/csynth.rpt` (regenerate with
`v++ -c --mode hls --config synth_k8.cfg --work_dir work_k8_report`).

| metric | value |
|---|---|
| Initiation interval (coding loop) | **II = 1** |
| Estimated Fmax | **273.97 MHz** (closes the 200 MHz target with margin) |
| LUT | 43,573 (**37%**) |
| FF | 24,339 (10%) |
| DSP | 48 (3%) |
| BRAM | 26 (9%) |

**On-fabric result** (this bitstream, `../board/`): **13.29 M sym/s = 3.84× the
ARM Cortex-A53**, lossless (host decoded the board's output == input). Co-sim
predicted 12.9 M/s → within ~3% of hardware, because the kernel compresses a whole
4 KB buffer per call so launch overhead amortises. Raw log:
`../board/onfabric_result.txt`.

DSP note: the 48 DSPs are the 8 lanes × the `range*prob` interval multiply — the
multiply that iteration 2 (interleaving) and the multiply-free directions target.
