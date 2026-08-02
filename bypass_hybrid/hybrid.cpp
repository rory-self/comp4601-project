/*
 * Entropy-classified HYBRID coder (Idea 4).
 *
 * The profiler measures H(bit|ctx) for each of the 8 bit-tree levels. A level
 * with H ~ 1 is incompressible GIVEN its context: the adaptive model cannot beat
 * 1 bit/bin, so paying the arithmetic recurrence for it is wasted work.
 *
 *   adaptive levels (ADAPT_MASK bit set) -> M-coder arithmetic bin (recurrence,
 *                                           ~1 cycle each, the throughput cost)
 *   bypass  levels                       -> RAW bit, packed straight out
 *                                           (costs exactly 1 bit = optimal for
 *                                            H=1, and is trivially parallel)
 *
 * Throughput is set by the ADAPTIVE bin count: cycles/byte drops from 8 to
 * popcount(ADAPT_MASK). Compression is unchanged (we only stop modelling bins
 * the model could not predict anyway).
 *
 * Two output streams so the decoder can separate them; their lengths are in the
 * header. The decoder knows the mask, so it knows which stream each bin is in.
 */
#include "mcoder.h"
#include <string.h>

#ifndef ADAPT_MASK          /* bit L set => level L (MSB-first) is adaptive */
#define ADAPT_MASK 0xFF     /* default: all 8 adaptive == plain M-coder     */
#endif
#define IS_ADAPT(l) ((ADAPT_MASK >> (l)) & 1)

#define HY_MAX_IN  4096
#define HY_MAX_OUT 8192

int hybrid_encode(const mc_byte *in, int n, mc_byte *out) {
    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;

    static mc_byte abuf[HY_MAX_OUT];      /* arithmetic stream */
    static mc_byte rbuf[HY_MAX_OUT];      /* raw bypass stream */
    mc_enc e; mc_enc_init(&e, abuf);
    uint32_t racc = 0; int rn = 0, ri = 0;

    for (int k = 0; k < n; k++) {
        int b = in[k], ctx = 1;
        for (int l = 0; l < 8; l++) {
            int bit = (b >> (7 - l)) & 1;
            if (IS_ADAPT(l)) {
                mc_encode_bin(&e, &tree[ctx], bit);      /* modelled */
            } else {
                racc = (racc << 1) | bit; rn++;          /* raw 1 bit */
                if (rn == 8) { rbuf[ri++] = (mc_byte)racc; racc = 0; rn = 0; }
            }
            ctx = (ctx << 1) | bit;                      /* context still walks */
        }
    }
    mc_enc_flush(&e);
    if (rn) { rbuf[ri++] = (mc_byte)(racc << (8 - rn)); }

    int alen = e.pk.oi, oi = 0;
    out[oi++] = (mc_byte)(alen & 0xff); out[oi++] = (mc_byte)((alen >> 8) & 0xff);
    out[oi++] = (mc_byte)(ri   & 0xff); out[oi++] = (mc_byte)((ri   >> 8) & 0xff);
    out[oi++] = (mc_byte)(n    & 0xff); out[oi++] = (mc_byte)((n    >> 8) & 0xff);
    memcpy(out + oi, abuf, alen); oi += alen;
    memcpy(out + oi, rbuf, ri);   oi += ri;
    return oi;
}

int hybrid_decode(const mc_byte *comp, int comp_len, mc_byte *out) {
    int alen = comp[0] | (comp[1] << 8);
    int rlen = comp[2] | (comp[3] << 8);
    int n    = comp[4] | (comp[5] << 8);
    (void)rlen; (void)comp_len;
    const mc_byte *ab = comp + 6, *rb = comp + 6 + alen;

    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
    mc_dec d; mc_dec_init(&d, ab, alen);
    long rpos = 0;

    for (int k = 0; k < n; k++) {
        int b = 0, ctx = 1;
        for (int l = 0; l < 8; l++) {
            int bit;
            if (IS_ADAPT(l)) {
                bit = mc_decode_bin(&d, &tree[ctx]);
            } else {
                bit = (rb[rpos >> 3] >> (7 - (rpos & 7))) & 1; rpos++;
            }
            b = (b << 1) | bit; ctx = (ctx << 1) | bit;
        }
        out[k] = (mc_byte)b;
    }
    return n;
}
