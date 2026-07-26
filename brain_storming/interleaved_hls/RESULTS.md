# Time-interleaved HLS arithmetic coder

This iteration attacks the loop-carried arithmetic-coder dependency without
weakening the probability model and without instantiating one complete datapath
per stream.

## Architecture

Independent coder states are visited round-robin by a shared micro-operation
pipeline. With enough contexts between two visits to the same state, HLS sees a
longer recurrence distance and schedules the `Interleave` loop at aggregate
**II=1**.

`GROUPS` controls the number of physical shared pipelines. `LANES=16` preserves
the original K=16 chunking and 255-context adaptive model:

| configuration | contexts per engine | clock | RTL cycles, 4095 B | throughput | vs ARM | LUT |
|---|---:|---:|---:|---:|---:|---:|
| 1 engine | 16 | 200 MHz | 111,961 | 7.315 M/s | 2.11x | 20,863 (17%) |
| 4 engines | 4 | 200 MHz | 39,606 | 20.679 M/s | 5.98x | 22,072 (18%) |
| 8 engines | 2 | 150 MHz | 27,544 | **22.301 M/s** | **6.45x** | 24,702 (21%) |

All figures are cycle-accurate Verilog co-simulation results. Every result
passed C post-checking and lossless decode. The repeating-pattern test remains
1904 bytes (46.50%), exactly matching the original K=16 model.

The eight-engine design does not meet 200 MHz: HLS estimates 158.05 MHz. It was
therefore synthesized and measured against a conservative 150 MHz clock. The
four-engine design meets 200 MHz with an estimated 273.97 MHz Fmax and is the
safer board candidate.

## Comparison with the earlier approaches

| design | throughput | LUT | model |
|---|---:|---:|---|
| original physically replicated K=16 | 16.8 M/s | 71% | 255-context tree |
| lean physically replicated K=31 | 28.2 M/s | 95% | reduced 8-probability model |
| interleaved 4-engine | 20.679 M/s | 18% | 255-context tree |
| interleaved 8-engine | 22.301 M/s | 21% | 255-context tree |

The interleaved result is the stronger architectural contribution: it improves
throughput per LUT substantially while preserving compression quality. The
K=31 experiment remains the absolute HLS-throughput upper bound, but obtains it
through near-device-filling replication and a weaker model.

## Memory-access experiment

The normal byte-pointer design already infers AXI burst reads. A separate
64-bit input experiment packed eight pixels per external word and gathered
unaligned chunk words on chip.

| four-engine input path | cycles | throughput | LUT | BRAM |
|---|---:|---:|---:|---:|
| byte pointer with inferred bursts | 39,606 | 20.679 M/s | 18% | 17% |
| explicit 64-bit gather | 52,635 | 15.560 M/s | 22% | 23% |

The explicit wide path is **32.9% slower**, so it is retained as a negative
result rather than the recommended kernel. Unaligned gathering at each chunk
boundary costs more than widening saves and increases on-chip storage.

For the recommended four-engine design, approximate serialized work is:

- input staging: about 4095 cycles;
- header and compressed output staging: about 1936 cycles on the pattern;
- total RTL latency: 39,606 cycles.

Thus current data movement represents at most about 15% of latency; arithmetic
micro-operations remain dominant. Perfectly overlapping all staging could not
improve this case by more than roughly 1.18x. A future streaming board design
should only be pursued with a changed striped input layout or multiple AXI
channels, because simply widening the existing contiguous-chunk layout loses.

## Reproduction

```sh
# Software lossless test
g++ -O2 -Wno-unused-label -Wno-unknown-pragmas \
  -DLANES=16 -DGROUPS=4 \
  arith_interleaved.cpp interleaved_test.cpp -o interleaved_test
./interleaved_test

# Recommended 200 MHz candidate
v++ -c --mode hls --config interleaved_g4.cfg --work_dir work_g4
vitis-run --mode hls --cosim --config interleaved_g4.cfg --work_dir work_g4

# Higher-throughput 150 MHz candidate
v++ -c --mode hls --config interleaved_g8_150.cfg --work_dir work_g8_150
vitis-run --mode hls --cosim --config interleaved_g8_150.cfg \
  --work_dir work_g8_150
```

