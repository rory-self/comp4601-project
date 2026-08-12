#!/bin/bash
# Rebuild the mcoder board kernel at an arbitrary MC_MAX_IN, against the
# platform this board's own working firmware uses (kv260_custom, confirmed by
# xclbinutil against the running bitstream -- not the AMD base platform).
#
#   ./build_size.sh <MC_MAX_IN bytes> <tag>
#   ./build_size.sh 32768 32k
#
# MC_MAX_IN sizes the buf[][]/cout[][] staging arrays and is compiled into the
# RTL, so a larger block needs a rebuild -- the host cannot request more than
# the kernel was built for.  Per-chunk rlen/clen are uint16 in the container
# header (mcoder.h: MC_HDR_BYTES), so the real ceiling is a per-chunk size of
# 65535 bytes, i.e. MC_MAX_IN < 65535 * MC_KWAY = ~512 KB at K=8 -- 32 KB and
# 64 KB are nowhere near it (chunk = MC_MAX_IN/8 = 4096 / 8192 bytes).
cd "$(dirname "$0")"

# Source the toolchain BEFORE set -u -- settings64.sh reads unset vars such as
# LD_LIBRARY_PATH and would abort the script under nounset.
source /tools/Xilinx/2025.2/Vitis/settings64.sh >/dev/null 2>&1
set -uo pipefail
N="${1:?usage: build_size.sh <MC_MAX_IN> <tag>}"
TAG="${2:?usage: build_size.sh <MC_MAX_IN> <tag>}"
PLATFORM=/home/bb/WorkSpace/xilinx-zynqmp-common-v2025.2/kv260_custom/export/kv260_custom/kv260_custom.xpfm
[ -f "$PLATFORM" ] || { echo "FATAL: platform not found: $PLATFORM"; exit 1; }

BOARD_CFG=/tmp/mc_board_$TAG.cfg
LINK_CFG=/tmp/mc_link_$TAG.cfg

sed -e "s|^syn.cflags=.*|syn.cflags=-DMC_KWAY=8 -DMC_CTX_LUTRAM -DMC_MAX_IN=$N -I../src|" \
    -e "s|^tb.cflags=.*|tb.cflags=-DMC_KWAY=8 -DMC_CTX_LUTRAM -DMC_MAX_IN=$N -I../src|" \
    board.cfg > "$BOARD_CFG"

sed -e "s|^temp_dir=.*|temp_dir=bc1_$TAG|" \
    -e "s|^report_dir=.*|report_dir=bc1_$TAG/reports|" \
    -e "s|^log_dir=.*|log_dir=bc1_$TAG/logs|" \
    -e "s|solution_name=.*|solution_name=bc1_$TAG|" link.cfg > "$LINK_CFG"

echo "=== [1/3] HLS synthesis, MC_MAX_IN=$N  ($(date +%H:%M:%S))"
rm -rf work_board_$TAG
v++ -c --mode hls --config "$BOARD_CFG" --work_dir work_board_$TAG \
    > /tmp/mc_${TAG}_synth.log 2>&1
if ! grep -q "Finished Generating all RTL models" /tmp/mc_${TAG}_synth.log; then
    echo "SYNTH FAILED -- tail of /tmp/mc_${TAG}_synth.log:"
    tail -30 /tmp/mc_${TAG}_synth.log
    exit 1
fi
grep -E "Estimated Fmax|Loop Constraint Status" /tmp/mc_${TAG}_synth.log | tail -2

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

echo "=== [2/3] C-sim at the full block size ($(date +%H:%M:%S))"
CSIM_CFG=/tmp/mc_csim_$TAG.cfg
sed -e "s|^tb.cflags=.*|tb.cflags=-DMC_KWAY=8 -DMC_CTX_LUTRAM -DMC_MAX_IN=$N -DTB_N=$N -I../src|" \
    "$BOARD_CFG" > "$CSIM_CFG"
vitis-run --mode hls --csim --config "$CSIM_CFG" --work_dir work_board_$TAG \
    > /tmp/mc_${TAG}_csim.log 2>&1
if ! grep -q "PASS: lossless" /tmp/mc_${TAG}_csim.log; then
    echo "CSIM DID NOT PASS -- tail of /tmp/mc_${TAG}_csim.log:"
    tail -20 /tmp/mc_${TAG}_csim.log
    exit 1
fi
grep -E "^(text|random|all-zero) " /tmp/mc_${TAG}_csim.log

echo "=== [3/3] link -> bitstream  ($(date +%H:%M:%S))  -- this is the slow one"
v++ --link --target hw --platform "$PLATFORM" --config "$LINK_CFG" \
    -o ../bin/arith_$TAG.xclbin "$XO" > /tmp/mc_${TAG}_link.log 2>&1
RC=$?
if [ $RC -ne 0 ] || [ ! -f "../bin/arith_$TAG.xclbin" ]; then
    echo "LINK FAILED (rc=$RC) -- tail of /tmp/mc_${TAG}_link.log:"
    tail -40 /tmp/mc_${TAG}_link.log
    exit 1
fi
cp ../bin/arith_$TAG.xclbin ../bin/arith_$TAG.bin
rm -f ../bin/arith_$TAG.xclbin   # byte-identical to .bin; only .bin is tracked

echo "=== DONE  ($(date +%H:%M:%S))  -> bin/arith_$TAG.bin"
xclbinutil --info --input ../bin/arith_$TAG.bin 2>/dev/null | grep -i "Platform VBNV"
R2=$(find bc1_$TAG -name "*timing_summary_routed.rpt" -path "*reports*" 2>/dev/null | head -1)
if [ -n "$R2" ]; then
  grep -A8 "^| Design Timing Summary" "$R2" | grep -E "^ +[0-9-]+\.[0-9]+" | head -1
  sed -n '/^Clock  *Waveform/,/^$/p' "$R2" | grep -E "clk_out"
fi
