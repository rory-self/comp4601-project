#!/bin/bash
# Image-compression demo: compress + reconstruct an image, ARM CPU vs FPGA kernel,
# entirely on the KV260. Shows an ASCII preview, the compression ratio, that the
# reconstruction is pixel-perfect (lossless), and the CPU-vs-FPGA speedup.
#
# Assumes:
#   - board reachable over ssh as petalinux@<BOARD_IP>
#   - the arith bitstream (../board/arith.bin) and demo binary (demo_arm) are built
#
# Usage:  ./run_demo.sh <BOARD_IP>          (default 10.42.0.25)

set -e
BOARD=${1:-10.42.0.25}
USER=petalinux
HERE=$(dirname "$0")

echo ">> copying demo + bitstream + image to the board"
scp "$HERE/demo_arm" "$HERE/image.pgm" "$HERE/../board/arith.bin" "$USER@$BOARD:/home/$USER/"

echo ">> loading the FPGA bitstream (needs sudo on the board)"
ssh "$USER@$BOARD" '
  sudo mkdir -p /lib/firmware/xilinx/arith
  sudo cp pl.dtbo shell.json arith.bin /lib/firmware/xilinx/arith/
  sudo xmutil unloadapp >/dev/null 2>&1 || true
  sudo xmutil loadapp arith
'

echo ">> running the demo"
ssh "$USER@$BOARD" 'cd /home/petalinux && chmod +x demo_arm && ./demo_arm -i image.pgm -x arith.bin'

echo ">> pulling the reconstructed image back"
scp "$USER@$BOARD:/home/$USER/reconstructed.pgm" "$HERE/"
cmp -s "$HERE/image.pgm" "$HERE/reconstructed.pgm" \
  && echo "reconstructed.pgm is byte-identical to image.pgm (lossless confirmed)" \
  || echo "WARNING: reconstructed differs from original"
