# Phase 1: plain C++ validation of the M-coder.  No Vitis needed.
#
#   make            build + run at K=1 (directly comparable to the 83.7
#                   cycles/byte hardware baseline in results/sweep_big_results.csv)
#   make isolate    run at K=1 with V5's 9-bin structure, so the reported
#                   compression delta is the ROM quantisation cost alone
#   make sweep      run at K = 1, 2, 4, 8, 16
#   make K=8 run    any single K

CXX      ?= g++
# The HLS pragmas and loop labels in mcoder.h are meaningful only to Vitis;
# g++ correctly reports them as unknown/unused, so silence those two and keep
# the rest of -Wall -Wextra live.
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++11 -Wno-unknown-pragmas -Wno-unused-label
K        ?= 1
FLAG     ?=

SRC  = mcoder_enc.cpp mcoder_dec.cpp mcoder_test.cpp ref/v5_prof.cpp
DEFS = -DMC_PROFILE -DMC_KWAY=$(K) -DV5_KWAY=$(K) $(FLAG)
TAG  = k$(K)$(if $(FLAG),_flag,)
BIN  = build/mcoder_test_$(TAG)

.PHONY: all run isolate sweep clean
all: run

$(BIN): $(SRC) mcoder.h mcoder_tables.h ref/v5_prof.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEFS) $(SRC) -o $@

run: $(BIN)
	@./$(BIN)

isolate:
	@$(MAKE) --no-print-directory K=1 FLAG=-DMC_TERM_FLAG run

sweep:
	@for k in 1 2 4 8 16; do $(MAKE) --no-print-directory K=$$k run; echo; done

clean:
	rm -rf build
