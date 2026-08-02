# Image-compression demo for the V7 M-coder.
#
#   make SYSROOT=<xrt-sysroot>   cross-compile for the board -> mc_demo_arm
#   make sim                     build + run on this machine, no board
#
# K must match the kernel build (hls/hls_board.cfg uses MC_KWAY=8).

K        ?= 8
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-label
DEFS      = -DMC_KWAY=$(K) -I..

SRC = mc_demo.cpp ../mcoder_enc.cpp ../mcoder_dec.cpp

CROSS   ?= /tools/Xilinx/2025.2/Vitis/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++
SYSROOT ?=
IMG     ?= ../../demo/image.pgm

.PHONY: all sim clean
all: mc_demo_arm

mc_demo_arm: $(SRC) ../mcoder.h ../mcoder_tables.h
	@test -n "$(SYSROOT)" || { echo "set SYSROOT=<xrt sysroot>, or use 'make sim'"; exit 1; }
	$(CROSS) $(CXXFLAGS) $(DEFS) --sysroot=$(SYSROOT) \
	  -I$(SYSROOT)/usr/include -I$(SYSROOT)/usr/include/xrt \
	  $(SRC) -o $@ \
	  -L$(SYSROOT)/usr/lib -lxrt_coreutil -lpthread -lrt -ldl -luuid

mc_demo_sim: $(SRC) ../mcoder.h ../mcoder_tables.h
	g++ $(CXXFLAGS) $(DEFS) -DMC_NO_XRT $(SRC) -o $@

sim: mc_demo_sim
	./mc_demo_sim -i $(IMG)

clean:
	rm -f mc_demo_arm mc_demo_sim reconstructed.pgm
