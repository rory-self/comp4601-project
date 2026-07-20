#!/bin/bash
cd /home/sadat-kabir/Work/COMP4601/labs/Project/arith_sandbox
source /home/sadat-kabir/Xilinx/2025.2.1/Vitis/settings64.sh >/dev/null 2>&1
export XILINXD_LICENSE_FILE=/home/sadat-kabir/.Xilinx/Xilinx.lic
echo "K,cycles_256sym,cyc_per_sym,LUT,LUT_pct,BRAM,DSP" > sweep_results.csv
for K in 1 2 4 8 16; do
  cat > cfg_k$K.cfg <<EOF
part=xck26-sfvc784-2LV-c

[hls]
flow_target=vitis
package.output.format=xo
package.output.syn=false
syn.top=arith_encode
syn.file=arith5_k$K.cpp
syn.file=arith3.h
tb.file=arith5_cosim.cpp
tb.file=arith3.h
clock=5ns
EOF
  rm -rf sweep_k$K
  timeout 300 v++ -c --mode hls --config cfg_k$K.cfg --work_dir sweep_k$K > sweep_c$K.log 2>&1
  timeout 400 vitis-run --mode hls --cosim --config cfg_k$K.cfg --work_dir sweep_k$K > sweep_cosim$K.log 2>&1
  RPT=$(find sweep_k$K -iname "hls_cosim.rpt" 2>/dev/null | head -1)
  CYC=$([ -n "$RPT" ] && grep -iE "Verilog" "$RPT" | grep -oE "[0-9]{3,}" | head -1)
  CS=$(find sweep_k$K -iname csynth.rpt 2>/dev/null | head -1)
  LINE=$([ -n "$CS" ] && grep -E "^\|\+ arith_encode" "$CS" | head -1)
  LUT=$(echo "$LINE" | grep -oE "[0-9]+ \([0-9]+%\)" | sed -n '4p' | grep -oE "^[0-9]+")
  LUTP=$(echo "$LINE" | grep -oE "[0-9]+ \([0-9]+%\)" | sed -n '4p' | grep -oE "[0-9]+%")
  BRAM=$(echo "$LINE" | grep -oE "[0-9]+ \([0-9~]+%?\)" | sed -n '1p' | grep -oE "^[0-9]+")
  DSP=$(echo "$LINE" | grep -oE "[0-9]+ \([0-9~]+%?\)" | sed -n '2p' | grep -oE "^[0-9]+")
  CPS=$([ -n "$CYC" ] && python3 -c "print(f'{$CYC/256:.1f}')" 2>/dev/null)
  echo "$K,$CYC,$CPS,$LUT,$LUTP,$BRAM,$DSP" >> sweep_results.csv
  echo "done K=$K: cyc=$CYC cyc/sym=$CPS LUT=$LUT($LUTP)"
done
echo "SWEEP COMPLETE"
