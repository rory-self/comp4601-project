# Runbook: synthesise → build xclbin → deploy → run on the KV260

End-to-end command reference for the interleaved kernel (`arith_kernel`), captured
from the working session. Fill in the two placeholders once:

```sh
VITIS=$HOME/Xilinx/2025.2.1/Vitis                 # Vitis install (has settings64.sh)
BOARD=petalinux@10.42.0.25                         # board ssh target (password: <board pw>)
source "$VITIS/settings64.sh"
```

Paths on THIS machine (already correct):

```sh
PLATFORM=$HOME/Work/COMP4601/labs/workspace/kv260_custom/export/kv260_custom/kv260_custom.xpfm
SYSROOT=$HOME/Work/COMP4601/labs/workspace/xilinx-zynqmp-common-v2025.2/sysroots/cortexa72-cortexa53-amd-linux
CXX=$HOME/Xilinx/2025.2.1/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++
```

---

## 1. HLS: C source → `.xo`  (two steps — package is separate)

```sh
cd board_iter2
# g4 (200 MHz, safe). For the 400 MHz attempt swap in hls_g2.cfg everywhere.
v++     -c    --mode hls --config hls_g4.cfg --work_dir work_hls          # csynth + RTL
vitis-run --mode hls --package --config hls_g4.cfg --work_dir work_hls    # -> work_hls/arith_kernel.xo
```

Optional correctness check before the long link:

```sh
vitis-run --mode hls --cosim --config hls_g4.cfg --work_dir work_hls
# expect: pattern in=4095 coded=1904 ... OK   and   PASS
```

Read the Fmax / resource numbers from `work_hls/hls/syn/report/csynth.rpt`
(or grep `Estimated Fmax` in the v++ log).

## 2. Link: `.xo` → `.xclbin`  (the long Vivado step, ~30–60 min)

```sh
v++ --link --target hw --platform "$PLATFORM" --config link.cfg \
    -o arith.xclbin work_hls/arith_kernel.xo
```

Clock is chosen in `link.cfg` → `[clock] id=N:arith_kernel_1`
(**0 = 100 MHz, 1 = 200 MHz, 2 = 400 MHz**). `link_400.cfg` is the id=2 version.
`build.sh` runs steps 1+2 in one shot: `./build.sh [hls_g4.cfg] [link.cfg]`.

## 3. Cross-compile the host (aarch64)

The XRT host runs on the board. Compile it here against the Yocto sysroot.

```sh
# NOTE: do NOT `source environment-setup-*` — that script has a stale --sysroot
# path (points at an old lab1/ location). Invoke the compiler directly instead:
"$CXX" -std=c++20 -O2 --sysroot="$SYSROOT" -I"$SYSROOT/usr/include" \
    host.cpp -o arith_host_v2 \
    -L"$SYSROOT/usr/lib" -lxrt_coreutil -lpthread -luuid
file arith_host_v2   # -> ELF 64-bit ARM aarch64
```

`host.cpp` decodes **16 lanes** (the interleaved kernel splits into LANES=16
chunks). The K-way host used 8 — don't reuse it or the lossless check fails.

## 4. Deploy to the board

```sh
scp arith.xclbin  "$BOARD":/tmp/arith_v2.xclbin
scp arith_host_v2 "$BOARD":/tmp/
ssh "$BOARD" 'chmod +x /tmp/arith_host_v2'
```

## 5. Load the bitstream (Kria XRT_FLAT app)

The PL is programmed via `xmutil`, which reads `/lib/firmware/xilinx/arith/arith.bin`.
Swap our xclbin in there (keep the existing `pl.dtbo` + `shell.json` — same
platform, same interface, so they still apply). Needs root (`sudo`, board pw).

```sh
ssh -t "$BOARD" '
  sudo cp -n /lib/firmware/xilinx/arith/arith.bin /lib/firmware/xilinx/arith/arith.bin.bak
  sudo cp /tmp/arith_v2.xclbin /lib/firmware/xilinx/arith/arith.bin
  sudo xmutil unloadapp
  sudo xmutil loadapp arith '            # -> "arith: Loaded with slot_handle 0"
```

To go back to the K=8 replicated bitstream: `sudo cp .../arith.bin.bak .../arith.bin`
then `unloadapp; loadapp arith`.

## 6. Run + profile

```sh
ssh "$BOARD" 'cd /tmp; export XILINX_XRT=/usr;
  ./arith_host_v2 -x /tmp/arith_v2.xclbin -N 4095 -n 2000'
```

Flags: `-x` xclbin, `-N` symbols/call (≤4095), `-n` profiling iterations,
`-d` device (0). Output = lossless PASS/FAIL, compressed size, and per-call
mean/median µs + M sym/s (chrono around kernel enqueue+wait only).

Overhead sweep (separates fixed XRT cost from per-symbol compute):

```sh
ssh "$BOARD" 'cd /tmp; export XILINX_XRT=/usr;
  for N in 256 1024 2048 4095; do ./arith_host_v2 -x /tmp/arith_v2.xclbin -N $N -n 1500 \
    | grep -E "throughput|per-symbol"; done'
```

---

## Gotchas (all hit this session)

- **Package is a separate command.** `--package` belongs to `vitis-run`, not `v++`.
  `v++ -c` alone leaves you with RTL but no `.xo`.
- **Stale SDK sysroot.** The env-setup script's `--sysroot` points at a moved
  path (`lab1/…`); call the compiler directly with the real `$SYSROOT` (step 3).
- **16-lane decoder.** Interleaved output has a 16-entry length header; the host
  must use `kKWay = 16`.
- **XRT buffer indices.** Args are `arith_kernel(in, n, out, out_len)` → buffer
  BOs sit at `group_id(0)`, `group_id(2)`, `group_id(3)`; index 1 is the scalar
  `n`. Using `group_id(1)` for a buffer throws `out_of_range`.
- **`xclbinutil --info --input arith.xclbin`** shows the baked-in clock id and the
  kernel signature — use it to confirm which variant a given `.xclbin` is.
