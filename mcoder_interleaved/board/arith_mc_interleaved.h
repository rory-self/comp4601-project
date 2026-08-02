#ifndef ARITH_MC_INTERLEAVED_H_
#define ARITH_MC_INTERLEAVED_H_

#include "mcoder.h"   // Bryan's M-coder primitives + tables (mc_code_bin, etc.)

// Interleaved M-coder: LANES independent CABAC coder states are visited
// round-robin by GROUPS shared micro-op pipelines. Same output format as
// mc_encode (header [LANES x {u16 rlen, u16 clen}] + chunks), so mc_decode
// (compiled with MC_KWAY == LANES) verifies it losslessly.

int arith_mc_encode_interleaved(const mc_byte in[MC_MAX_IN], int n,
                                mc_byte out[MC_MAX_OUT]);

#endif
