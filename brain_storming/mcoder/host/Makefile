# XRT host for the V7 M-coder kernel.
#
#   make SYSROOT=<path>   cross-compile for the board (aarch64)
#   make sim              build+run a host-logic test on this machine, no XRT
#
# K must match the kernel build (hls/hls_board.cfg uses MC_KWAY=8).

K        ?= 8
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-label
# -I.. resolves "mcoder.h"; MC_KWAY must match the kernel build.
DEFS      = -DMC_KWAY=$(K) -I..

SRC  = mc_host.cpp ../mcoder_dec.cpp
SIM_SRC = $(SRC) ../mcoder_enc.cpp

# --- board (aarch64) ---
CROSS   ?= aarch64-linux-gnu-g++
SYSROOT ?=

.PHONY: all sim clean
all: mc_host_arm

mc_host_arm: $(SRC) ../mcoder.h ../mcoder_tables.h
	@test -n "$(SYSROOT)" || { echo "set SYSROOT=<xrt sysroot>, or use 'make sim'"; exit 1; }
	$(CROSS) $(CXXFLAGS) $(DEFS) --sysroot=$(SYSROOT) \
	  -I$(SYSROOT)/usr/include -I$(SYSROOT)/usr/include/xrt \
	  $(SRC) -o $@ \
	  -L$(SYSROOT)/usr/lib -lxrt_coreutil -lpthread -lrt -ldl -luuid

# --- host-logic test, no XRT, no board ---
mc_host_sim: $(SIM_SRC) ../mcoder.h ../mcoder_tables.h
	g++ $(CXXFLAGS) $(DEFS) -DMC_NO_XRT $(SIM_SRC) -o $@

sim: mc_host_sim
	./mc_host_sim -n 200 -f ../../demo/image.pgm

clean:
	rm -f mc_host_arm mc_host_sim
