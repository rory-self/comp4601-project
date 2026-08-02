/* HLS top for the entropy-classified hybrid coder.
 * Adaptive bins (ADAPT_MASK) pay the arithmetic recurrence; bins the model
 * cannot beat (H(bit|ctx) ~ 1) are packed raw. Throughput is set by
 * popcount(ADAPT_MASK), so cycles/byte falls from 8 to the adaptive count. */
#include "mcoder.h"

#ifndef ADAPT_MASK
#define ADAPT_MASK 0xFF
#endif
#define IS_ADAPT(l) ((ADAPT_MASK >> (l)) & 1)
#define HY_MAX_IN  4096
#define HY_MAX_OUT 8192

int hybrid_encode_hls(const mc_byte in[HY_MAX_IN], int n, mc_byte out[HY_MAX_OUT]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif
    static mc_byte buf[HY_MAX_IN], abuf[HY_MAX_OUT], rbuf[HY_MAX_OUT];
    mc_ctx tree[MC_NTREE];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=tree complete
#endif
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
Load:
    for (int i = 0; i < n; i++) buf[i] = in[i];

    mc_enc e; mc_enc_init(&e, abuf);
    uint32_t racc = 0; int rn = 0, ri = 0;

Bytes:
    for (int k = 0; k < n; k++) {
        int b = buf[k], ctx = 1;
    Levels:
        for (int l = 0; l < 8; l++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            int bit = (b >> (7 - l)) & 1;
            if (IS_ADAPT(l)) {
                mc_encode_bin(&e, &tree[ctx], bit);     /* modelled: the recurrence */
            } else {
                racc = (racc << 1) | bit; rn++;         /* raw: 1 bit, no model     */
                if (rn == 8) { rbuf[ri++] = (mc_byte)racc; racc = 0; rn = 0; }
            }
            ctx = (ctx << 1) | bit;
        }
    }
    mc_enc_flush(&e);
    if (rn) rbuf[ri++] = (mc_byte)(racc << (8 - rn));

    int alen = e.pk.oi, oi = 0;
    out[oi++] = (mc_byte)(alen & 0xff); out[oi++] = (mc_byte)((alen >> 8) & 0xff);
    out[oi++] = (mc_byte)(ri   & 0xff); out[oi++] = (mc_byte)((ri   >> 8) & 0xff);
    out[oi++] = (mc_byte)(n    & 0xff); out[oi++] = (mc_byte)((n    >> 8) & 0xff);
Copy1:
    for (int i = 0; i < alen; i++) out[oi++] = abuf[i];
Copy2:
    for (int i = 0; i < ri; i++)   out[oi++] = rbuf[i];
    return oi;
}
