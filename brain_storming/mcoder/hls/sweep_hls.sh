#!/bin/bash
# Phase 3: synthesise + co-simulate the kernel across K, same shape as
# best_hls/sweep.sh.  Writes ../results/mcoder_hls_sweep.csv.
#
# cyc/byte is measured by cosim on the TB corpus (5130 bytes over 6 cases),
# not estimated, so it includes the AXI copy loops and the packer.
cd "$(dirname "$0")"
# Source the toolchain BEFORE any `set -u`: settings64.sh reads unset vars
# such as LD_LIBRARY_PATH and would abort the script under nounset.
source /tools/Xilinx/2025.2/Vitis/settings64.sh >/dev/null 2>&1
set -u
mkdir -p ../results
OUT=../results/mcoder_hls_sweep.csv
TB_BYTES=5130
echo "K,clock_ns,timing_viol,bin_II,cosim_total_cyc,cyc_per_byte,LUT,LUT_pct,FF,BRAM,DSP" > "$OUT"

for K in 1 2 4 8 16; do
    CFG=/tmp/mc_hls_k$K.cfg
    sed -e "s/^syn.cflags=.*/syn.cflags=-DMC_KWAY=$K/" \
        -e "s/^tb.cflags=.*/tb.cflags=-DMC_KWAY=$K/" hls_config.cfg > "$CFG"
    rm -rf work_k$K
    timeout 3600 v++ -c --mode hls --config "$CFG" --work_dir work_k$K > /tmp/mc_syn_$K.log 2>&1
    R=work_k$K/hls/syn/report/csynth.rpt
    [ -f "$R" ] || { echo "$K,,SYNTH_FAILED,,,,,,,," >> "$OUT"; continue; }

    VIOL=$(grep -E '^\|' "$R" | grep -c Timing)
    II=$(grep -oE "Final II = [0-9]+, Depth = [0-9]+, loop 'Bins'" /tmp/mc_syn_$K.log | grep -oE "II = [0-9]+" | head -1 | grep -oE "[0-9]+")
    TOP=$(grep -E "^\|\+ mc_encode " "$R" | head -1)
    BRAM=$(echo "$TOP" | awk -F'|' '{print $12}' | grep -oE "^ *[0-9]+" | tr -d ' ')
    DSP=$(echo  "$TOP" | awk -F'|' '{print $13}' | grep -oE "[0-9]+" | head -1)
    FF=$(echo   "$TOP" | awk -F'|' '{print $14}' | grep -oE "^ *[0-9]+" | tr -d ' ')
    LUT=$(echo  "$TOP" | awk -F'|' '{print $15}' | grep -oE "^ *[0-9]+" | tr -d ' ')
    LUTP=$(echo "$TOP" | awk -F'|' '{print $15}' | grep -oE "[0-9]+%")

    timeout 3600 vitis-run --mode hls --cosim --config "$CFG" --work_dir work_k$K > /tmp/mc_cosim_$K.log 2>&1
    CYC=$(grep -E "Verilog" work_k$K/reports/hls_cosim.rpt 2>/dev/null | awk -F'|' '{print $10}' | tr -d ' ')
    CPB=$([ -n "${CYC:-}" ] && python3 -c "print(f'{$CYC/$TB_BYTES:.2f}')" 2>/dev/null)

    echo "$K,6.0,$VIOL,${II:-},${CYC:-},${CPB:-},${LUT:-},${LUTP:-},${FF:-},${BRAM:-0},${DSP:-0}" >> "$OUT"
    echo "K=$K viol=$VIOL II=${II:-?} cyc=${CYC:-?} cyc/byte=${CPB:-?} LUT=${LUT:-?}(${LUTP:-?}) DSP=${DSP:-0}"
done
echo
column -s, -t < "$OUT"
