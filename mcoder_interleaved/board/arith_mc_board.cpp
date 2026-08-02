/* Board top for the interleaved M-coder: wraps arith_mc_encode_interleaved and
 * exposes the compressed length via an m_axi out_len port (BOARD_WRAP suppresses
 * the core's own interface pragmas so they are declared here, on the top). */
#include "arith_mc_interleaved.h"
int arith_mc_encode_interleaved(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
void arith_kernel(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT], int out_len[1]) {
#pragma HLS INTERFACE m_axi port=in      offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out     offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
    out_len[0] = arith_mc_encode_interleaved(in, n, out);
}
