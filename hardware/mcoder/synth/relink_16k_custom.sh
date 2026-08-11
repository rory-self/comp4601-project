#!/bin/bash
# Relink the 16 KB .xo against kv260_custom -- the SAME platform the board's
# working mcoder firmware was built with (VBNV xilinx_kv260_kv260_hardware_
# platform_0_0, confirmed with xclbinutil against the running bitstream).
#
# An earlier detour relinked this against the AMD base platform on the mistaken
# belief that the custom platform was wrong.  It is not: replication_full /
# interleaved / tans use the base platform, but mcoder uses the custom one.
cd "$(dirname "$0")"
source /tools/Xilinx/2025.2/Vitis/settings64.sh >/dev/null 2>&1
set -uo pipefail

TAG=16k
PLATFORM=/home/bb/WorkSpace/xilinx-zynqmp-common-v2025.2/kv260_custom/export/kv260_custom/kv260_custom.xpfm
XO=work_board_$TAG/arith_kernel.xo
[ -f "$XO" ] || { echo "FATAL: $XO missing"; exit 1; }

# Generated config goes to /tmp, as sweep.sh does.
LINK_CFG=/tmp/mc_link_${TAG}c.cfg
sed -e "s|^temp_dir=.*|temp_dir=bc1_${TAG}c|" \
    -e "s|^report_dir=.*|report_dir=bc1_${TAG}c/reports|" \
    -e "s|^log_dir=.*|log_dir=bc1_${TAG}c/logs|" \
    -e "s|solution_name=.*|solution_name=bc1_${TAG}c|" link.cfg > "$LINK_CFG"

echo "=== link against kv260_custom ($(date +%H:%M:%S))"
rm -rf bc1_${TAG}c
v++ --link --target hw --platform "$PLATFORM" --config "$LINK_CFG" \
    -o ../bin/arith_${TAG}.xclbin "$XO" > /tmp/mc_${TAG}c_link.log 2>&1
RC=$?
if [ $RC -ne 0 ] || [ ! -f "../bin/arith_${TAG}.xclbin" ]; then
    echo "LINK FAILED (rc=$RC):"; tail -30 /tmp/mc_${TAG}c_link.log; exit 1
fi
cp ../bin/arith_${TAG}.xclbin ../bin/arith_${TAG}.bin

echo "=== DONE ($(date +%H:%M:%S))"
xclbinutil --info --input ../bin/arith_${TAG}.bin 2>/dev/null | grep -i "Platform VBNV" | head -1
R=$(find bc1_${TAG}c -name "*timing_summary_routed.rpt" -path "*reports*" 2>/dev/null | head -1)
if [ -n "$R" ]; then
  echo "--- WNS / kernel clock:"
  grep -A3 "WNS(ns)" "$R" | tail -1
  sed -n '/^Clock  *Waveform/,/^$/p' "$R" | grep -E "clk_out" | head -2
fi
