#!/bin/bash
# Sweep K and emit results/mcoder_sweep.csv, same spirit as best_hls/sweep.sh.
set -e
cd "$(dirname "$0")"
mkdir -p results
OUT=results/mcoder_sweep.csv
echo "K,term_flag,bytes,v5_comp,mc_comp,v5_cyc_per_byte,mc_cyc_per_byte,size_delta_pct" > "$OUT"
for k in 1 2 4 8 16; do
    for flag in "" "-DMC_TERM_FLAG"; do
        make --no-print-directory K=$k FLAG="$flag" run 2>/dev/null \
            | grep '^CSV,' | cut -d, -f2- >> "$OUT"
    done
done
column -s, -t < "$OUT"
echo
echo "wrote $OUT"
