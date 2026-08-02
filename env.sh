#!/usr/bin/env bash
# Portable environment for building/running this project on ANY machine.
#
#   source ./env.sh
#
# It auto-detects what it can and tells you exactly what to set if it can't.
# Override anything by exporting it first, e.g.:
#   VITIS_ROOT=/tools/Xilinx/2025.2 source ./env.sh

_ok=1
_here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export PROJ_ROOT="$_here"

# ---------------------------------------------------------------- Vitis -----
if [ -z "${VITIS_ROOT:-}" ]; then
  for c in "$XILINX_VITIS" "$HOME/Xilinx"/*/Vitis /tools/Xilinx/Vitis/* /opt/Xilinx/Vitis/*; do
    [ -x "$c/bin/v++" ] && { VITIS_ROOT="$c"; break; }
  done
  # also accept a versioned root that contains Vitis/
  if [ -z "${VITIS_ROOT:-}" ]; then
    for c in "$HOME/Xilinx"/*; do [ -x "$c/Vitis/bin/v++" ] && { VITIS_ROOT="$c/Vitis"; break; }; done
  fi
fi
if [ -n "${VITIS_ROOT:-}" ] && [ -f "$VITIS_ROOT/settings64.sh" ]; then
  # shellcheck disable=SC1091
  source "$VITIS_ROOT/settings64.sh"
  export VITIS_ROOT
  echo "  Vitis     : $VITIS_ROOT  ($(v++ --version 2>/dev/null | grep -oE 'v[0-9]+\.[0-9]+' | head -1))"
  export VITIS_INCLUDE="$VITIS_ROOT/include"          # ap_int.h, hls_stream.h
else
  echo "  Vitis     : NOT FOUND -- set VITIS_ROOT=/path/to/Vitis (the dir with settings64.sh)"; _ok=0
fi

# ------------------------------------------------------------- platform -----
# A KV260 platform (.xpfm). We used a custom one built in the labs; the stock
# AMD base platform also works for linking.
if [ -z "${PLATFORM:-}" ]; then
  for c in "$PROJ_ROOT"/../../workspace/kv260_custom/export/kv260_custom/kv260_custom.xpfm \
           "$HOME"/*/kv260_custom/export/kv260_custom/kv260_custom.xpfm \
           "$VITIS_ROOT"/base_platforms/xilinx_kv260_base_*/xilinx_kv260_base_*.xpfm; do
    [ -f "$c" ] && { PLATFORM="$(readlink -f "$c")"; break; }
  done
fi
if [ -n "${PLATFORM:-}" ]; then export PLATFORM; echo "  Platform  : $PLATFORM"
else echo "  Platform  : NOT FOUND -- set PLATFORM=/path/to/<kv260>.xpfm"; _ok=0; fi

# -------------------------------------------------------------- sysroot -----
# Yocto/PetaLinux sysroot supplying XRT headers+libs for the aarch64 host.
if [ -z "${SYSROOT:-}" ]; then
  for c in "$PROJ_ROOT"/../../workspace/xilinx-zynqmp-common-*/sysroots/cortexa72-cortexa53-* \
           "$HOME"/*/xilinx-zynqmp-common-*/sysroots/cortexa72-cortexa53-* \
           /opt/petalinux/*/sysroots/cortexa72-cortexa53-*; do
    [ -f "$c/usr/include/xrt/xrt_kernel.h" ] && { SYSROOT="$(readlink -f "$c")"; break; }
  done
fi
if [ -n "${SYSROOT:-}" ]; then export SYSROOT; echo "  Sysroot   : $SYSROOT"
else echo "  Sysroot   : NOT FOUND -- set SYSROOT=/path/to/sysroots/cortexa72-cortexa53-*"; _ok=0; fi

# --------------------------------------------------- aarch64 cross-compiler --
# NOTE: do NOT source the SDK's environment-setup-* script -- in our install its
# --sysroot points at a stale path. Invoke the compiler directly instead.
if [ -z "${CXX_ARM:-}" ]; then
  for c in "$VITIS_ROOT"/../gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++ \
           "$(command -v aarch64-linux-gnu-g++ 2>/dev/null)" \
           "$(command -v aarch64-none-linux-gnu-g++ 2>/dev/null)"; do
    [ -n "$c" ] && [ -x "$c" ] && { CXX_ARM="$(readlink -f "$c")"; break; }
  done
fi
if [ -n "${CXX_ARM:-}" ]; then export CXX_ARM; echo "  ARM g++   : $CXX_ARM"
else echo "  ARM g++   : NOT FOUND -- set CXX_ARM=/path/to/aarch64-linux-gnu-g++"; _ok=0; fi

# ------------------------------------------------------------------ board ----
export BOARD="${BOARD:-petalinux@10.42.0.25}"
export BOARD_PW="${BOARD_PW:-petalinux1}"
echo "  Board     : $BOARD   (override: BOARD=user@ip BOARD_PW=...)"

# convenience: one flag set for every host cross-compile
export ARM_CXXFLAGS="-std=c++20 -O3 -mcpu=cortex-a53 -Wno-unknown-pragmas --sysroot=$SYSROOT -I$SYSROOT/usr/include"
export ARM_LDFLAGS="-L$SYSROOT/usr/lib -lxrt_coreutil -lpthread -luuid"

if [ "$_ok" = 1 ]; then echo "  -> environment OK"
else echo "  -> some paths missing: software builds still work; HLS/bitstream/host steps need them"; fi
unset _ok _here
