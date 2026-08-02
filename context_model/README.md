# context_model — what we should have been modelling (software study)

Every coder in this project models **one byte in isolation** (order-0). Images are
mostly **spatially** redundant. This measures what that choice costs, and what
taking the better model would cost in hardware.

## Build & run (only needs g++)
```sh
g++ -O2 ctx_study.cpp -o ctx_study
./ctx_study ../replication_full/demo/image.pgm ../replication_full/demo/img_smooth.pgm
```

## Result
| model | image.pgm | context memory |
|---|---:|---:|
| order-0 (what we built) | 6.734 b/sym → 84.2% | **0.4 KB** |
| spatial: pixel above | 2.188 → 27.4% | 90 KB |
| **MED predictor (JPEG-LS)** | **1.202 → 15.0%** | 91 KB |

**The finding is the trade, not the ratio.** Our bit-tree needs 255 probability
states *per context*. Order-0 = 1 context = 0.4 KB, which is why **16 lanes** fit and
why all our parallelism was affordable. A MED model needs ~242 contexts ≈ **91 KB per
lane**, so only ~2–3 lanes fit. **Compression and parallelism compete for the same
BRAM** — the ~4× ratio win would cost ~5–8× of the lane count.

Software-measured only; **not implemented in HLS**. See `RESULTS.md`.
