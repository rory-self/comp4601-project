/* Board top-level: wraps the K-way arithmetic encoder (arith5.cpp) and exposes
 * the compressed length through an m_axi out_len port so the host can read it. */
#include "arith3.h"
int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);  /* from arith5.cpp */

void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]) {
#pragma HLS INTERFACE m_axi port=in      offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out     offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
    out_len[0] = arith_encode(in, n, out);
}
