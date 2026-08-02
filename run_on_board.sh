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
SSHOPT=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)
bssh(){ SSH_ASKPASS="$TMP/ap" SSH_ASKPASS_REQUIRE=force setsid -w ssh "${SSHOPT[@]}" "$BOARD" "$@"; }
bscp(){ SSH_ASKPASS="$TMP/ap" SSH_ASKPASS_REQUIRE=force setsid -w scp "${SSHOPT[@]}" "$@"; }

# ---- design registry:  key | dir | bitstream | host binary | extra files | args
designs=(
  "rep|replication_full|board/arith.bin|board/arith_host_arm||-N 4095 -n ITERS"
  "interleaved|interleaved|board/arith.xclbin|board/demo_host_arm||-N 4095 -n ITERS"
  "tans|tans|board/arith.xclbin|board/tans_host_arm|demo/file0.bin demo/file1.bin demo/file2.bin demo/file3.bin|-d /tmp -n 200"
  "multi|tans|board/arith_multi.xclbin|board/multi_host_arm|demo/file0.bin|-d /tmp -n 300"
)
declare -A DESC=(
  [rep]="arith, K=8 replicated        (iteration 1, the first fabric design)"
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
  dir="$HERE/$(field "$spec" 2)"
  bit="$dir/$(field "$spec" 3)"; host="$dir/$(field "$spec" 4)"
  extra="$(field "$spec" 5)";    args="$(field "$spec" 6)"; args="${args//ITERS/$ITERS}"

  echo "=============================================================="
  echo " $key -- ${DESC[$key]}"
  echo "=============================================================="
  for f in "$bit" "$host"; do
    [ -f "$f" ] || { echo "  MISSING: $f"; echo "  (build it first -- see $dir/DEMO.md)"; return 1; }
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

if [ "$1" = "all" ]; then
  SUM="$TMP/summary"; : > "$SUM"
  for d in "${designs[@]}"; do
    k=$(field "$d" 1)
    out="$(run_one "$k" 2>&1)"; echo "$out"
    # scrape whatever throughput/verdict line each host prints.
    # hosts differ: "M sym/s" (tans) vs "M symbols/s" (rep) -- match both.
    tp=$(echo "$out" | grep -oiE "[0-9.]+ M sym[a-z]*/s" | tail -1)
    sp=$(echo "$out" | grep -oiE "SPEEDUP[^:]*: *[0-9.]+x?" | grep -oE "[0-9.]+x?$" | tail -1)
    ok=$(echo "$out" | grep -ciE "lossless *: *YES|PASS:|LOSSLESS")
    printf "%-12s|%-14s|%-9s|%s\n" "$k" "${tp:-n/a}" "${sp:-n/a}" \
           "$([ "$ok" -gt 0 ] && echo PASS || echo CHECK)" >> "$SUM"
  done
  echo "================== COMPARISON (this run) ====================="
  printf "%-12s %-14s %-9s %s\n" "DESIGN" "THROUGHPUT" "SPEEDUP" "CORRECT"
  printf "%-12s %-14s %-9s %s\n" "------" "----------" "-------" "-------"
  while IFS='|' read -r a b c d; do printf "%-12s %-14s %-9s %s\n" "$a" "$b" "$c" "$d"; done < "$SUM"
  echo "=============================================================="
  echo "Note: speedups are vs each design's OWN software baseline, and those"
  echo "baselines differ (bit-wise arith 3.5 M/s vs byte-wise tANS 56 M/s),"
  echo "so compare THROUGHPUT across rows, not the ratios. See DEMO_CHEATSHEET.md."
else
  run_one "$1"
fi
