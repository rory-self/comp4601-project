# Synthesis results — mcoder (K=8, contexts in LUTRAM, 6.0 ns HLS constraint)

    Bins loop        II = 1, depth 7
    Pack loop        II = 1, depth 2
    Timing           0 violating rows
    BRAM             42 (14%)
    DSP              0
    FF               13170 (5%)
    LUT              43165 (36%)

## Post-route (Vivado impl_1, 200 MHz)

    WNS 0.754 ns   TNS 0.000   0 failing endpoints of 71960

The HLS constraint is 6.0 ns but the **KV260 platform exposes only fixed clocks
(100 / 199.998 / 400 MHz)**, so the kernel is clocked at 5.000 ns regardless --
`clock=6.0ns` only steers HLS scheduling, not the hardware clock. HLS's own
estimate at 5 ns was -0.61 ns; the real post-route path is 3.996 ns with 2 logic
levels, 56% of it routing. HLS was pessimistic (it reserves 12.5% clock
uncertainty and estimates routing pre-placement).

The post-route critical path lands in the `Concat` output copy loop, **not** in
the coder.

## K sweep

`results/mcoder_hls_sweep.csv` — every one of the 10 configurations closes
timing with II=1. Two context-storage variants are swept: `ctxreg` (partitioned
register file) and `ctxram` (distributed RAM).

| variant | K | cosim cycles | cyc/byte | LUT | LUT % | DSP |
|---|---|---|---|---|---|---|
| ctxreg | 1 | 73578 | 14.34 | 11243 | 9% | 0 |
| ctxreg | 4 | 35292 | 6.88 | 35530 | 30% | 0 |
| ctxreg | 8 | 32694 | 6.37 | 67349 | 57% | 0 |
| ctxreg | 16 | 39505 | 7.70 | 131000 | **111% — does not fit** | 2 |
| ctxram | 1 | 75120 | 14.64 | 8049 | 6% | 0 |
| ctxram | 4 | 36834 | 7.18 | 22754 | 19% | 0 |
| **ctxram** | **8** | **34236** | **6.67** | **41797** | **35%** | **0** |
| ctxram | 16 | 41047 | 8.00 | 79896 | 68% | 2 |

`ctxram` at K=8 is the shipped configuration: **-38% LUT for +4.7% cycles.**

Returns collapse well before K=16: K=1->2 buys 36%, K=2->4 buys 24%, K=4->8
buys 7%, and K=8->16 is *negative*. That is the AXI copy loops, which move the
same total bytes regardless of K.
