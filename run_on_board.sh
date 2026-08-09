#!/usr/bin/env bash
# run_on_board.sh -- deploy a design to the KV260, run it, report timing + correctness.
#
#   ./run_on_board.sh                 list the designs
#   ./run_on_board.sh tans            run one design
#   ./run_on_board.sh all             run every design and print a comparison table
#
# Env overrides:  BOARD=petalinux@10.42.0.25  BOARD_PW=petalinux1  ITERS=2000
set -uo pipefail

BOARD="${BOARD:-petalinux@10.42.0.25}"
BOARD_PW="${BOARD_PW:-petalinux1}"
ITERS="${ITERS:-2000}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW=/lib/firmware/xilinx/arith/arith.bin      # the Kria app slot we swap into

# ---- ssh/scp without an interactive password prompt -------------------------
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
printf '#!/bin/sh\necho %s\n' "$BOARD_PW" > "$TMP/ap"; chmod +x "$TMP/ap"
SSHOPT=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1)
# setsid+SSH_ASKPASS feeds the password without a TTY. If you use SSH keys instead,
# this is harmless. If `setsid` is missing (e.g. macOS), fall back to plain ssh and
# let it prompt.
if command -v setsid >/dev/null 2>&1; then
  bssh(){ SSH_ASKPASS="$TMP/ap" SSH_ASKPASS_REQUIRE=force setsid -w ssh "${SSHOPT[@]}" "$BOARD" "$@"; }
  bscp(){ SSH_ASKPASS="$TMP/ap" SSH_ASKPASS_REQUIRE=force setsid -w scp "${SSHOPT[@]}" "$@"; }
else
  bssh(){ ssh "${SSHOPT[@]}" "$BOARD" "$@"; }
  bscp(){ scp "${SSHOPT[@]}" "$@"; }
fi

# ---- preflight: fail with something actionable, not "scp failed" ------------
preflight(){
  if ! bssh true >/dev/null 2>&1; then
    cat >&2 <<MSG
Cannot reach the board over ssh as: $BOARD

  * different IP or user?   BOARD=user@ip ./run_on_board.sh ...
  * different password?     BOARD_PW=yourpw ./run_on_board.sh ...
  * board powered on and on the network? try:  ping ${BOARD#*@}
  * using ssh keys? that works too -- this script only supplies a password
                    when the board asks for one.
MSG
    return 1
  fi
  if ! bssh "test -d $(dirname $FW)" >/dev/null 2>&1; then
    echo "Board reachable, but $(dirname $FW) is missing." >&2
    echo "  The Kria 'arith' firmware slot is not installed on this board." >&2
    return 1
  fi
  return 0
}

# ---- design registry:  key | dir | bitstream | host binary | extra files | args
# field 7 = TIMING BOUNDARY the reported throughput uses. NEVER compare across
# boundaries: "demo" includes host<->device transfer, "kernel" does not.
designs=(
  "rep|replication_full|bin/arith.bin|bin/demo_arm|data/image.pgm|-i image.pgm|demo"
  "mcoder|mcoder|bin/arith.bin|bin/demo_arm|data/image.pgm data/text_page.pgm|-i image.pgm|demo"
  "interleaved|interleaved|bin/arith.xclbin|bin/demo_host_arm||-N 4095 -n ITERS|kernel"
  "tans|tans|bin/arith.xclbin|bin/demo_host_arm|data/file0.bin data/file1.bin data/file2.bin data/file3.bin|-d /tmp -n 200|kernel"
  "multi|tans|bin/arith_multi.xclbin|bin/multi_host_arm|data/file0.bin|-d /tmp -n 300|kernel"
)
declare -A DESC=(
  [rep]="arith, K=8 replicated        (image demo: CPU vs FPGA, pixel-perfect)"
  [mcoder]="table-driven M-coder, K=8    (no multiply, 0 DSP -- 2.49x rep on fabric)"
  [interleaved]="arith, C-slow interleaved     (iteration 2, area-efficient)"
  [tans]="tree method, static tANS     (shared frequency table, SIMD + wide AXI)"
  [multi]="tANS x2 compute units        (2 CUs on separate HP ports -- 1.95x scaling)"
)

field(){ echo "$1" | cut -d'|' -f"$2"; }
find_design(){ for d in "${designs[@]}"; do [ "$(field "$d" 1)" = "$1" ] && { echo "$d"; return 0; }; done; return 1; }

usage(){
  echo "Usage: $0 <design|all>"; echo
  printf "  %-12s %s\n" "DESIGN" "WHAT IT IS"
  for d in "${designs[@]}"; do k=$(field "$d" 1); printf "  %-12s %s\n" "$k" "${DESC[$k]}"; done
  echo; echo "  all          run every design, then print a comparison table"
  echo; echo "Board: $BOARD   (override with BOARD=..., BOARD_PW=..., ITERS=...)"
}

run_one(){
  local key="$1" spec dir bit host extra args
  spec="$(find_design "$key")" || { echo "unknown design '$key'"; return 1; }
  dir="$HERE/hardware/$(field "$spec" 2)"
  bit="$dir/$(field "$spec" 3)"; host="$dir/$(field "$spec" 4)"
  extra="$(field "$spec" 5)";    args="$(field "$spec" 6)"; args="${args//ITERS/$ITERS}"

  echo "=============================================================="
  echo " $key -- ${DESC[$key]}"
  echo "=============================================================="
  for f in "$bit" "$host"; do
    [ -f "$f" ] || { echo "  MISSING: $f"; echo "  (build it first -- see $dir/README.md)"; return 1; }
  done

  echo "-- deploying $(basename "$bit") + $(basename "$host")"
  bscp "$bit"  "$BOARD:/tmp/run_$key.bin" >/dev/null 2>&1 || { echo "  scp failed"; return 1; }
  bscp "$host" "$BOARD:/tmp/run_$key"     >/dev/null 2>&1 || { echo "  scp failed"; return 1; }
  if [ -n "$extra" ]; then (cd "$dir" && bscp $extra "$BOARD:/tmp/" >/dev/null 2>&1); fi

  echo "-- loading bitstream onto the PL"
  bssh "echo $BOARD_PW | sudo -S cp /tmp/run_$key.bin $FW 2>/dev/null
        echo $BOARD_PW | sudo -S xmutil unloadapp >/dev/null 2>&1
        echo $BOARD_PW | sudo -S xmutil loadapp arith 2>/dev/null" | tail -1

  echo "-- running (this reports timing AND verifies the output is lossless)"
  echo
  bssh "cd /tmp && chmod +x run_$key && XILINX_XRT=/usr ./run_$key -x $FW $args" 2>&1 \
    | grep -viE "^open device|^load xclbin" | sed 's/^/   /'
  echo
}

# ---- main -------------------------------------------------------------------
[ $# -eq 0 ] && { usage; exit 0; }
preflight || exit 1

if [ "$1" = "all" ]; then
  SUM="$TMP/summary"; : > "$SUM"
  for d in "${designs[@]}"; do
    k=$(field "$d" 1)
    out="$(run_one "$k" 2>&1)"; echo "$out"
    # scrape whatever throughput/verdict line each host prints.
    # hosts differ: "M sym/s" (tans) vs "M symbols/s" (rep) -- match both.
    tp=$(echo "$out" | grep -oiE "[0-9.]+ M sym[a-z]*/s" | tail -1)
    [ -z "$tp" ] && tp=$(echo "$out" | grep -oiE "FPGA[^(]*\(([0-9.]+) MB/s" | grep -oE "[0-9.]+ MB/s" | tail -1)
    sp=$(echo "$out" | grep -oiE "SPEEDUP[^:]*: *[0-9.]+x?" | grep -oE "[0-9.]+x?$" | tail -1)
    # multi_host reports CU scaling rather than a software speedup
    [ -z "$sp" ] && sp=$(echo "$out" | grep -oiE "scaling *: *[0-9.]+x" | grep -oE "[0-9.]+x" | tail -1)
    ok=$(echo "$out" | grep -ciE "lossless *: *YES|PASS:|LOSSLESS|all CUs identical *: *YES")
    # these coders emit one symbol per byte, so MB/s and M sym/s are the same
    # number -- normalise the unit so the column is readable.
    tp=$(echo "$tp" | sed -E 's/ M(B|| sym[a-z]*)\/s/ M sym\/s/; s/ M symbols\/s/ M sym\/s/')
    bnd=$(field "$d" 7)
    printf "%-12s|%-14s|%-8s|%-9s|%s\n" "$k" "${tp:-n/a}" "$bnd" "${sp:-n/a}" \
           "$([ "$ok" -gt 0 ] && echo PASS || echo CHECK)" >> "$SUM"
  done
  echo "===================== COMPARISON (this run) ========================"
  printf "%-12s %-14s %-8s %-9s %s\n" "DESIGN" "THROUGHPUT" "TIMING" "VS SW*" "CORRECT"
  printf "%-12s %-14s %-8s %-9s %s\n" "------" "----------" "------" "-------" "-------"
  while IFS='|' read -r a b c d e; do printf "%-12s %-14s %-8s %-9s %s\n" "$a" "$b" "$c" "$d" "$e"; done < "$SUM"
  echo "===================================================================="
  cat <<'NOTE'
READ THE 'TIMING' COLUMN BEFORE COMPARING ANYTHING.

  kernel = chrono around enqueue+wait only.  Excludes host<->device transfer.
  demo   = the whole compress call.          INCLUDES that transfer.

Rows with different TIMING are NOT comparable. The same M-coder bitstream reads
33.1 M sym/s as 'kernel' and 22.3 M sym/s as 'demo'; both are correct. For the
like-for-like kernel figure on the arith designs, run bench_host instead:
  ssh $BOARD './bench_host_arm -x <bitstream> -N 4095 -n 2000'

* is vs each design's OWN software baseline, and those differ ~16x (bit-wise
arith ~3.5 M sym/s on the A53 vs byte-wise tANS ~56). So compare the THROUGHPUT
column within a TIMING group -- never the ratios. For 'multi' the figure is CU
scaling, not a speedup over software.
NOTE
  else
  run_one "$1"
fi
