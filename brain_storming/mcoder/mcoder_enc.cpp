/*
 * M-coder encoder, K-way multi-stream.  Structurally identical to
 * best_hls/arith5.cpp: split the input into K independent chunks, code them
 * with K concurrent engine instances, concatenate behind a length header.
 *
 * Phase 1 build: plain C++, no HLS pragmas.  The pragma-annotated Phase 2
 * kernel lives in mcoder_hls.cpp and shares this same engine header.
 */
#include <string.h>
#include "mcoder.h"

#ifdef MC_PROFILE
mc_prof_t g_mc_prof;
#endif

/* Encode one independent chunk.  Returns compressed byte length. */
int mc_encode_chunk(const mc_byte *in, int n, mc_byte *out) {
    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
#ifdef MC_TERM_FLAG
    mc_ctx flag = MC_CTX_INIT;
#endif

    mc_enc e;
    mc_enc_init(&e, out);

    for (int k = 0; k < n; k++) {
#ifdef MC_TERM_FLAG
        mc_encode_bin(&e, &flag, 1);        /* another symbol follows */
#endif
        int b = in[k], ctx = 1;
        /* 8-level bit-tree: MSB first, context = the bits decoded so far. */
        for (int j = 7; j >= 0; j--) {
            int bit = (b >> j) & 1;
            mc_encode_bin(&e, &tree[ctx], bit);
            ctx = (ctx << 1) | bit;
        }
    }
#ifdef MC_TERM_FLAG
    mc_encode_bin(&e, &flag, 0);            /* end of chunk */
#endif
    mc_enc_flush(&e);
    return e.pk.oi;
}

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]) {
    static mc_byte cout[MC_KWAY][MC_CHUNK_CAP];
    int rlen[MC_KWAY], clen[MC_KWAY];

    const int chunk = (n + MC_KWAY - 1) / MC_KWAY;

    /* K independent engines.  No shared state, so these are concurrent. */
    for (int c = 0; c < MC_KWAY; c++) {
        int start = c * chunk;
        int len   = n - start;
        if (len > chunk) len = chunk;
        if (len < 0)     len = 0;
        rlen[c] = len;
        clen[c] = mc_encode_chunk(in + start, len, cout[c]);
    }

    int oi = 0;
    for (int c = 0; c < MC_KWAY; c++) {
        out[oi++] = (mc_byte)( rlen[c]       & 0xFF);
        out[oi++] = (mc_byte)((rlen[c] >> 8) & 0xFF);
        out[oi++] = (mc_byte)( clen[c]       & 0xFF);
        out[oi++] = (mc_byte)((clen[c] >> 8) & 0xFF);
    }
    for (int c = 0; c < MC_KWAY; c++)
        for (int i = 0; i < clen[c]; i++) out[oi++] = cout[c][i];

    return oi;
}
