#!/bin/bash
# Rebuild a design's bitstream against kv260_custom -- the platform THIS board's
# firmware slots are configured for.
#
#   ./relink_for_board.sh <design-dir>          e.g. ./relink_for_board.sh replication_full
#
# Why this exists: the committed bitstreams for replication_full, interleaved
# and tans carry VBNV xilinx_kv260_som_som240_1_... (the AMD base platform),
# while every firmware slot on this board -- and mcoder's committed bitstream --
# is xilinx_kv260_kv260_hardware_platform_0_0 (kv260_custom).  Loading a
# base-platform bitstream through these slots "succeeds" as far as xmutil is
# concerned but the compute unit never runs: measured 0 bytes out and a flat
# 512 ms timeout per call.  The project's normal flow used a
# /lib/firmware/xilinx/arith/ slot which no longer exists on this board.
#
# Synthesis parameters come from each design's own synth/board.cfg, unchanged,
# so the rebuilt kernel is the same design -- only the link target differs.
cd "$(dirname "$0")"
DESIGN="${1:?usage: relink_for_board.sh <design-dir>}"
[ -d "$DESIGN/synth" ] || { echo "FATAL: $DESIGN/synth not found"; exit 1; }
cd "$DESIGN/synth"

source /tools/Xilinx/2025.2/Vitis/settings64.sh >/dev/null 2>&1
set -uo pipefail
PLATFORM=/home/bb/WorkSpace/xilinx-zynqmp-common-v2025.2/kv260_custom/export/kv260_custom/kv260_custom.xpfm
[ -f "$PLATFORM" ] || { echo "FATAL: platform not found"; exit 1; }

TAG=custom
LINK_CFG=/tmp/${DESIGN}_link_$TAG.cfg
sed -e "s|^temp_dir=.*|temp_dir=bc1_$TAG|" \
    -e "s|^report_dir=.*|report_dir=bc1_$TAG/reports|" \
    -e "s|^log_dir=.*|log_dir=bc1_$TAG/logs|" \
    -e "s|solution_name=.*|solution_name=bc1_$TAG|" link.cfg > "$LINK_CFG"

echo "=== [1/3] HLS synthesis ($DESIGN)  $(date +%H:%M:%S)"
rm -rf work_$TAG
v++ -c --mode hls --config board.cfg --work_dir work_$TAG > /tmp/${DESIGN}_synth.log 2>&1
grep -q "Finished Generating all RTL models" /tmp/${DESIGN}_synth.log || {
    echo "SYNTH FAILED:"; tail -25 /tmp/${DESIGN}_synth.log; exit 1; }
grep -E "Estimated Fmax|Loop Constraint Status" /tmp/${DESIGN}_synth.log | tail -2

echo "=== [2/3] package .xo  $(date +%H:%M:%S)"
vitis-run --mode hls --package --config board.cfg --work_dir work_$TAG \
    > /tmp/${DESIGN}_pkg.log 2>&1
XO=$(find work_$TAG -name "*.xo" | head -1)
[ -n "$XO" ] || { echo "PACKAGE FAILED:"; tail -20 /tmp/${DESIGN}_pkg.log; exit 1; }
echo "    $XO"

echo "=== [3/3] link against kv260_custom  $(date +%H:%M:%S)  -- slow"
v++ --link --target hw --platform "$PLATFORM" --config "$LINK_CFG" \
    -o ../bin/arith_custom.xclbin "$XO" > /tmp/${DESIGN}_link.log 2>&1
RC=$?
[ $RC -eq 0 ] && [ -f ../bin/arith_custom.xclbin ] || {
    echo "LINK FAILED (rc=$RC):"; tail -30 /tmp/${DESIGN}_link.log; exit 1; }
cp ../bin/arith_custom.xclbin ../bin/arith_custom.bin
rm -f ../bin/arith_custom.xclbin

echo "=== DONE $(date +%H:%M:%S) -> $DESIGN/bin/arith_custom.bin"
xclbinutil --info --input ../bin/arith_custom.bin 2>/dev/null | grep -i "Platform VBNV"
R=$(find bc1_$TAG link_tmp_$TAG -name "*timing_summary_routed.rpt" -path "*reports*" 2>/dev/null | head -1)
[ -n "$R" ] && grep -A8 "^| Design Timing Summary" "$R" | grep -E "^ +[0-9-]+\.[0-9]+" | head -1
