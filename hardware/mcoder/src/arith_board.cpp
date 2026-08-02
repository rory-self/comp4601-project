/*
 * Board top-level, deliberately signature-compatible with V5's
 * board/arith_board.cpp:
 *
 *     void arith_kernel(const byte in[MAX_IN], int n, byte out[MAX_OUT],
 *                       int out_len[1])
 *
 * Same name, same four arguments, same gmem0/gmem1/gmem2 bundles.  That means
 * the existing XRT host's plumbing works unchanged -- same kernel lookup, same
 * buffer objects, same argument indices (in=0, n=1, out=2, out_len=3) -- and
 * board/link.cfg's `nk=arith_kernel:1:arith_kernel_1` needs no edit either.
 *
 * `mc_encode` returns the length through ap_return (an AXI-Lite register);
 * copying it into the out_len m_axi buffer here is exactly what V5 does, and
 * keeps the host from having to read the return register.
 *
 * TWO THINGS STILL DIFFER, and neither is optional:
 *
 *   1. The host-side decoder must be the M-coder one (../mcoder_dec.cpp), not
 *      V5's.  Both the container layout and the probability model changed:
 *      V5 emits [K x uint16 comp_len] and terminates on a per-symbol
 *      continuation flag; this emits [K x {uint16 raw_len, uint16 comp_len}]
 *      and terminates on the raw length.  V5's dec_kway() will produce garbage
 *      on this stream -- it will not fail loudly.
 *
 *   2. The kernel clock is 6.0 ns (~167 MHz), not V5's 5 ns / 200 MHz, so the
 *      link step needs a matching kernel frequency.
 */
#include "mcoder.h"

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);

void arith_kernel(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT],
                  int out_len[1]) {
#pragma HLS INTERFACE m_axi port=in      offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out     offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
    out_len[0] = mc_encode(in, n, out);
}
