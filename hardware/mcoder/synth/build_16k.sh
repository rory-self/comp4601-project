#!/bin/bash
# Rebuild the mcoder board kernel with a 16 KB block (MC_MAX_IN=16384).
#
# MC_MAX_IN sizes the buf[][]/cout[][] staging arrays, so it is compiled into
# the RTL: the shipped 4 KB bitstream cannot take a larger block no matter what
# the host asks for.  This produces a second bitstream alongside it -- nothing
# here overwrites the 4 KB artifacts (separate work dir, separate output).
#
# Expect ~4x the staging BRAM: 42 of 288 BRAM18 at 4 KB, so ~154 (~53%) here.
# If it does not fit or does not close timing, that is the answer to "how far
# can the block grow", and the log says which.
cd "$(dirname "$0")"

# Source the toolchain BEFORE any `set -u` -- settings64.sh reads unset vars
# such as LD_LIBRARY_PATH and would abort the script under nounset.  Same trap
# sweep.sh documents.
source /tools/Xilinx/2025.2/Vitis/settings64.sh >/dev/null 2>&1
set -uo pipefail
PLATFORM=/home/bb/WorkSpace/xilinx-zynqmp-common-v2025.2/kv260_custom/export/kv260_custom/kv260_custom.xpfm
[ -f "$PLATFORM" ] || { echo "FATAL: platform not found: $PLATFORM"; exit 1; }

N=16384
TAG=16k

# Generated configs go to /tmp, as sweep.sh does -- they are derived from the
# tracked board.cfg/link.cfg and should not litter the source tree.
BOARD_CFG=/tmp/mc_board_$TAG.cfg
LINK_CFG=/tmp/mc_link_$TAG.cfg

# Same flags as board.cfg, plus the block size.
sed -e "s|^syn.cflags=.*|syn.cflags=-DMC_KWAY=8 -DMC_CTX_LUTRAM -DMC_MAX_IN=$N -I../src|" \
    -e "s|^tb.cflags=.*|tb.cflags=-DMC_KWAY=8 -DMC_CTX_LUTRAM -DMC_MAX_IN=$N -I../src|" \
    board.cfg > "$BOARD_CFG"

# Separate temp/report dirs so a concurrent 4 KB build cannot collide.
sed -e "s|^temp_dir=.*|temp_dir=bc1_$TAG|" \
    -e "s|^report_dir=.*|report_dir=bc1_$TAG/reports|" \
    -e "s|^log_dir=.*|log_dir=bc1_$TAG/logs|" \
    -e "s|solution_name=.*|solution_name=bc1_$TAG|" link.cfg > "$LINK_CFG"

echo "=== [1/2] HLS synthesis, MC_MAX_IN=$N  ($(date +%H:%M:%S))"
rm -rf work_board_$TAG
v++ -c --mode hls --config "$BOARD_CFG" --work_dir work_board_$TAG \
    > /tmp/mc_${TAG}_synth.log 2>&1
if ! grep -q "Finished Generating all RTL models" /tmp/mc_${TAG}_synth.log; then
    echo "SYNTH FAILED -- tail of /tmp/mc_${TAG}_synth.log:"
    tail -30 /tmp/mc_${TAG}_synth.log
    exit 1
fi
grep -E "Estimated Fmax|Loop Constraint Status" /tmp/mc_${TAG}_synth.log | tail -2

# Packaging is a separate invocation -- `v++ -c --mode hls` synthesises but does
# not emit the .xo (see the design's README build steps).
echo "    packaging .xo"
vitis-run --mode hls --package --config "$BOARD_CFG" --work_dir work_board_$TAG \
    > /tmp/mc_${TAG}_pkg.log 2>&1
XO=$(find work_board_$TAG -name "*.xo" | head -1)
if [ -z "$XO" ]; then
    echo "PACKAGE FAILED -- tail of /tmp/mc_${TAG}_pkg.log:"
    tail -25 /tmp/mc_${TAG}_pkg.log
    exit 1
fi
echo "    .xo built: $XO ($(stat -c%s "$XO") bytes)"

R=work_board_$TAG/hls/syn/report/csynth.rpt
if [ -f "$R" ]; then
    echo "    --- resource estimate (top) ---"
    grep -E "^\|\+ arith_kernel|^\|\+ mc_encode " "$R" | head -2
    echo "    --- timing violations: $(grep -E '^\|' "$R" | grep -c Timing)"
fi

echo "=== [2/2] link -> bitstream  ($(date +%H:%M:%S))  -- this is the slow one"
# v++ 2025.2 insists the link output is named .xclbin; the board's firmware slot
# wants a .bin.  Same file either way -- link, then copy to the slot's name.
v++ --link --target hw --platform "$PLATFORM" --config "$LINK_CFG" \
    -o ../bin/arith_$TAG.xclbin "$XO" > /tmp/mc_${TAG}_link.log 2>&1
RC=$?
if [ $RC -ne 0 ] || [ ! -f "../bin/arith_$TAG.xclbin" ]; then
    echo "LINK FAILED (rc=$RC) -- tail of /tmp/mc_${TAG}_link.log:"
    tail -40 /tmp/mc_${TAG}_link.log
    exit 1
fi
cp ../bin/arith_$TAG.xclbin ../bin/arith_$TAG.bin

echo "=== DONE  ($(date +%H:%M:%S))  -> bin/arith_$TAG.bin"
ls -la ../bin/arith_$TAG.bin
echo "--- post-route timing:"
grep -riE "WNS|Timing.*(met|violated)" bc1_$TAG/reports/*.rpt 2>/dev/null | head -5
find bc1_$TAG -name "*timing_summary*" 2>/dev/null | head -3
