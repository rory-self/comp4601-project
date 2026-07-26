# Maximum-throughput HLS arithmetic coder

This experiment asks a deliberately narrow question: how much lossless
arithmetic-coding throughput can fit in the KV260's K26 fabric according to
Vitis HLS synthesis, if compression ratio may be traded for lane count?

## Result

The largest design that fits the HLS resource estimate is **K=31**:

| design | BRAM18K | DSP | FF | LUT | LUT use | 4095-byte RTL latency |
|---|---:|---:|---:|---:|---:|---:|
| original K=16 | 50 | 98 | 46,274 | 83,335 | 71% | not rerun here |
| lean K=31 | 64 | 68 | 45,235 | 111,891 | 95% | 29,041 cycles |
| lean K=32 | 66 | 66 | 48,347 | 118,486 | 101% | does not fit |
| lean K=64 | 130 | 129 | 92,196 | 230,258 | 196% | does not fit |

At the 200 MHz project clock, K=31's cycle-accurate RTL co-simulation gives:

```
4095 symbols / (29041 cycles * 5 ns) = 28.20 M symbols/s
28.20 / 3.46 M ARM symbols/s         = 8.15x theoretical ARM speedup
```

The RTL co-simulation passed C post-checking and all encoded streams decoded
losslessly. Vitis HLS estimated a 3.650 ns clock path against the 5 ns target.

This is a **post-HLS theoretical result**, not a routed or on-board result.
At 95% estimated LUT use, K=31 is an edge-of-capacity experiment and may fail
placement/routing. K=28--30 is the more realistic range for a future bitstream.

## Architectural change

Each lane still performs adaptive binary arithmetic coding, but uses eight
probabilities (one per bit position) instead of the original 255-node prefix
tree. Interval and model state were also narrowed to their required 16-bit
widths. This reduces one lane from roughly 4,749 LUT / 6 DSP to roughly
3,389 LUT / 2 DSP and reduces model reset from 256 entries to eight.

The cost is compression efficiency:

| K=31 test | input | coded | ratio |
|---|---:|---:|---:|
| repeating `a`--`g` pattern | 4095 B | 2283 B | 55.75% |
| pseudo-random | 2048 B | 2258 B | 110.25% |
| 10-byte tiny input | 10 B | 103 B | 1030% |

The tiny-input overhead comes mainly from the 62-byte per-chunk length header.
This design should only be used for large buffers when throughput is the
primary metric.

## Reproduce

Load the Vitis environment, then run:

```sh
g++ -O2 -Wno-unknown-pragmas -DKWAY=31 \
  arith_max.cpp arith_max_test.cpp -o arith_max_test
./arith_max_test

v++ -c --mode hls --config k31_max.cfg --work_dir work_k31
vitis-run --mode hls --cosim --config k31_max.cfg --work_dir work_k31
```

The failed iteration-2 FSM is retained as a useful negative result: HLS
scheduled its main recurrence at II=25, so it is not a throughput improvement.

