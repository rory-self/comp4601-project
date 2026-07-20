#!/bin/bash
cd /home/sadat-kabir/Work/COMP4601/labs/Project/arith_sandbox
source /home/sadat-kabir/Xilinx/2025.2.1/Vitis/settings64.sh >/dev/null 2>&1
export XILINXD_LICENSE_FILE=/home/sadat-kabir/.Xilinx/Xilinx.lic
# 1024-symbol input to amortize per-chunk init
cat > cosim_big.cpp <<EOF
#include <stdio.h>
#include "arith3.h"
int arith_encode(const byte_t*, int, byte_t*);
static byte_t out[MAX_OUT];
int main(){ byte_t m[1024]; for(int i=0;i<1024;i++) m[i]='a'+(i%7);
    int c=arith_encode(m,1024,out); printf("1024 sym -> %d bytes\nPASS\n",c); return 0; }
EOF
echo "K,cyc_1024,cyc_per_sym,speedup_vs_K1" > sweep_big_results.csv
BASE=0
for K in 1 8 16; do
  cat > cfgb_k$K.cfg <<EOF
part=xck26-sfvc784-2LV-c

[hls]
flow_target=vitis
package.output.format=xo
package.output.syn=false
syn.top=arith_encode
syn.file=arith5_k$K.cpp
syn.file=arith3.h
tb.file=cosim_big.cpp
tb.file=arith3.h
clock=5ns
EOF
  rm -rf sweepb_k$K
  timeout 300 v++ -c --mode hls --config cfgb_k$K.cfg --work_dir sweepb_k$K > sweepb_c$K.log 2>&1
  timeout 500 vitis-run --mode hls --cosim --config cfgb_k$K.cfg --work_dir sweepb_k$K > sweepb_cosim$K.log 2>&1
  RPT=$(find sweepb_k$K -iname "hls_cosim.rpt" 2>/dev/null | head -1)
  CYC=$([ -n "$RPT" ] && grep -iE "Verilog" "$RPT" | grep -oE "[0-9]{3,}" | head -1)
  [ "$K" = "1" ] && BASE=$CYC
  SP=$([ -n "$CYC" ] && python3 -c "print(f'{$BASE/$CYC:.2f}')" 2>/dev/null)
  CPS=$([ -n "$CYC" ] && python3 -c "print(f'{$CYC/1024:.1f}')" 2>/dev/null)
  echo "$K,$CYC,$CPS,$SP" >> sweep_big_results.csv
  echo "done big K=$K: cyc=$CYC cyc/sym=$CPS speedup=$SP"
done
echo "BIG SWEEP COMPLETE"
