/* Board top-level for the time-interleaved encoder (iteration 2).
 * Wraps arith_encode_interleaved() and exposes the compressed length through an
 * m_axi out_len port, so the XRT host reads it exactly like the K-way kernel.
 * The core's own m_axi pragmas are suppressed (BOARD_WRAP) so the interfaces
 * are declared here, on the actual top. */
#include "arith_interleaved_v2.h"

int arith_encode_interleaved(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);

void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]) {
#pragma HLS INTERFACE m_axi port=in      offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out     offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
    out_len[0] = arith_encode_interleaved(in, n, out);
}
