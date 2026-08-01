#!/usr/bin/env bash
# Build the time-interleaved arithmetic-coding kernel into a KV260 bitstream.
#
#   ./build.sh                     # g4, 200 MHz  (SAFE, recommended)
#   ./build.sh hls_g2.cfg link_400.cfg   # g2, 400 MHz (max attempt, may fail timing)
#
# Output: arith.xclbin  (send this file back).
#
# Requirements on the build machine:
#   - Vitis 2025.2.1  (same version as the .xclbin consumer)
#   - the bundled ./platform/kv260_custom/kv260_custom.xpfm
# The Vivado place-&-route in step 2 is the long part (~30-60 min; faster CPU helps).
set -euo pipefail

# ---- EDIT if your paths differ -------------------------------------------
: "${VITIS:=$HOME/Xilinx/2025.2.1/Vitis}"                       # Vitis install dir
: "${PLATFORM:=$(pwd)/platform/kv260_custom/kv260_custom.xpfm}" # bundled platform
# --------------------------------------------------------------------------

HLS_CFG="${1:-hls_g4.cfg}"
LINK_CFG="${2:-link.cfg}"

echo "Vitis    : $VITIS"
echo "Platform : $PLATFORM"
echo "HLS cfg  : $HLS_CFG"
echo "Link cfg : $LINK_CFG"
[ -f "$VITIS/settings64.sh" ] || { echo "ERROR: set VITIS to your Vitis install"; exit 1; }
[ -f "$PLATFORM" ]            || { echo "ERROR: platform not found at $PLATFORM"; exit 1; }
# shellcheck disable=SC1091
source "$VITIS/settings64.sh"

rm -rf work_hls link_tmp arith.xclbin

echo "== 1/3  HLS compile (csynth + RTL) =="
v++ -c --mode hls --config "$HLS_CFG" --work_dir work_hls

echo "== 2/3  package -> .xo =="
vitis-run --mode hls --package --config "$HLS_CFG" --work_dir work_hls
XO=$(find work_hls -name '*.xo' | head -1)
[ -n "$XO" ] || { echo "ERROR: no .xo produced"; exit 1; }
echo "   .xo = $XO"

echo "== 3/3  link -> arith.xclbin  (this is the long Vivado step, ~30-60 min) =="
v++ --link --target hw --platform "$PLATFORM" --config "$LINK_CFG" -o arith.xclbin "$XO"

echo
echo "DONE.  Send back:  $(pwd)/arith.xclbin"
ls -lh arith.xclbin
