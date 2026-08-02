#!/usr/bin/env bash
# K-sweep for the replicated coder: synth + co-sim each K, collect cycles + resources.
#
#   source ../../env.sh      # once, sets up Vitis
#   ./sweep.sh               # default K = 1 2 4 8 16
#   ./sweep.sh 8 16          # or pick your own
#
# Portable: no absolute paths. Uses -DKWAY=K, so there is one source file, not one
# per K (the old version referenced arith5_k$K.cpp, which no longer exists).
set -uo pipefail
command -v v++ >/dev/null || { echo "v++ not on PATH -- run: source ../../env.sh"; exit 1; }
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; cd "$HERE"

KS=${*:-1 2 4 8 16}
OUT=sweep_results.csv
echo "K,cosim_cycles,cyc_per_sym,LUT,LUT_pct,BRAM,DSP" > "$OUT"

for K in $KS; do
  cfg="cfg_k$K.cfg"
  cat > "$cfg" <<CFG
part=xck26-sfvc784-2LV-c

[hls]
flow_target=vitis
package.output.format=xo
package.output.syn=false
syn.top=arith_encode
syn.file=arith5.cpp
syn.file=arith3.h
syn.cflags=-DKWAY=$K
tb.file=arith5_test.cpp
tb.file=arith3.h
tb.cflags=-DKWAY=$K
clock=5ns
CFG
  rm -rf "sweep_k$K"
  v++ -c --mode hls --config "$cfg" --work_dir "sweep_k$K" > "sweep_syn_k$K.log" 2>&1
  vitis-run --mode hls --cosim --config "$cfg" --work_dir "sweep_k$K" > "sweep_cosim_k$K.log" 2>&1

  RPT=$(find "sweep_k$K" -iname hls_cosim.rpt 2>/dev/null | head -1)
  CYC=$([ -n "$RPT" ] && grep -iE "Verilog" "$RPT" | awk -F'|' '{print $5}' | tr -d ' ')
  CS=$(find "sweep_k$K" -iname csynth.rpt 2>/dev/null | head -1)
  LINE=$([ -n "$CS" ] && grep -E "^\|\+ arith_encode" "$CS" | head -1)
  vals=$(echo "$LINE" | grep -oE "[0-9]+ \([0-9~]+%?\)")
  BRAM=$(echo "$vals" | sed -n 1p | grep -oE "^[0-9]+")
  DSP=$(echo  "$vals" | sed -n 2p | grep -oE "^[0-9]+")
  LUT=$(echo  "$vals" | sed -n 4p | grep -oE "^[0-9]+")
  LUTP=$(echo "$vals" | sed -n 4p | grep -oE "[0-9]+%")
  CPS=$([ -n "${CYC:-}" ] && awk "BEGIN{printf \"%.1f\", $CYC/4095}")
  echo "$K,${CYC:-NA},${CPS:-NA},${LUT:-NA},${LUTP:-NA},${BRAM:-NA},${DSP:-NA}" >> "$OUT"
  echo "K=$K  cycles=${CYC:-NA}  cyc/sym=${CPS:-NA}  LUT=${LUT:-NA} (${LUTP:-NA})"
done
echo "wrote $OUT"
