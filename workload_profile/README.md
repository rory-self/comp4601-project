# workload_profile — the classifiers that decide which technique applies

Two small tools. They are the front of the method: **measure the workload first,
then pick the acceleration technique** (and reject the ones the data rules out).

```
profile.cpp       order-0 entropy, per-bit-plane entropy, symbol skew, mean run length
cond_entropy.cpp  H(bit | ctx) per bit-tree level  <- the one that matters
```

## Build & run (only needs g++)
```sh
g++ -O2 profile.cpp      -o profile
g++ -O2 cond_entropy.cpp -o cond_entropy
./profile       ../replication_full/demo/*.pgm ../tans/demo/file0.bin
./cond_entropy  ../replication_full/demo/*.pgm ../tans/demo/file0.bin
```

## How to read the output
- **`H(bit|ctx) ≈ 1`** for a level ⇒ the adaptive model buys nothing there ⇒ route
  that bin to **bypass** (see `../bypass_hybrid/`, worth 2.02×–13.9× fewer cycles).
- **Stable histogram across files** ⇒ a **static table** is viable ⇒ the tree method
  (`../tans/`), which is byte-wise and much faster.
- **High `p_max` / long runs** ⇒ an MPS run-mode would pay. *Measured on our data:
  runs are 1.0–2.8 and `p_max ≤ 0.11`, so we rejected that idea without building it.*

Full interpretation and every idea it fed: `../EXPLORATION.md`.
