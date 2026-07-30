/*
 * INSTRUMENTED COPY of best_hls/arith5.cpp (+ the encode_bit/put_bit inlines
 * from best_hls/arith3.h) and the decoder from best_hls/arith5_test.cpp.
 * Arithmetic unchanged; counters added.  See v5_prof.h.
 */
#include <string.h>
#include "v5_prof.h"

v5_prof_t g_v5_prof;

static inline void put_bit(int bit, uint32_t &ob, int &nb, v5_byte *out, int &oi) {
    g_v5_prof.emit_calls++; g_v5_prof.emit_bits++;
    ob = (ob << 1) | (bit & 1); nb++;
    if (nb == 8) { out[oi++] = (v5_byte)ob; ob = 0; nb = 0; }
}

static inline void encode_bit(uint32_t &low, uint32_t &high, uint32_t &pending,
                              uint32_t &ob, int &nb, v5_byte *out, int &oi,
                              uint32_t &prob, int bit) {
    g_v5_prof.bins++;
    uint32_t range = high - low + 1;
    g_v5_prof.mults++;                                  /* the 17x12 multiply */
    uint32_t split = (range * prob) >> V5_PROB_BITS;
    if (bit == 0) high = low + split - 1;
    else          low  = low + split;

    for (int r = 0; r < 2 * V5_CODE_BITS; r++) {
        if (high < V5_HALF) {
            g_v5_prof.renorm_iters++;
            put_bit(0, ob, nb, out, oi);
            while (pending > 0) { put_bit(1, ob, nb, out, oi); pending--; g_v5_prof.pending_bits++; }
        } else if (low >= V5_HALF) {
            g_v5_prof.renorm_iters++;
            put_bit(1, ob, nb, out, oi);
            while (pending > 0) { put_bit(0, ob, nb, out, oi); pending--; g_v5_prof.pending_bits++; }
            low -= V5_HALF; high -= V5_HALF;
        } else if (low >= V5_FIRST_QTR && high < V5_THIRD_QTR) {
            g_v5_prof.renorm_iters++;
            pending++; low -= V5_FIRST_QTR; high -= V5_FIRST_QTR;
        } else break;
        low  = (low << 1) & V5_TOP_VALUE;
        high = ((high << 1) | 1) & V5_TOP_VALUE;
    }

    if (bit == 0) prob += (V5_PROB_TOTAL - prob) >> V5_MOVE_BITS;
    else          prob -= prob >> V5_MOVE_BITS;
}

static int encode_chunk(const v5_byte *in, int n, v5_byte *out) {
    uint32_t flag_prob = V5_PROB_INIT, tree[V5_NTREE];
    for (int i = 0; i < V5_NTREE; i++) tree[i] = V5_PROB_INIT;
    uint32_t ob = 0; int nb = 0, oi = 0;
    uint32_t low = 0, high = V5_TOP_VALUE, pending = 0;

    for (int k = 0; k < n; k++) {
        encode_bit(low, high, pending, ob, nb, out, oi, flag_prob, 1);
        int b = in[k], ctx = 1;
        for (int j = 7; j >= 0; j--) {
            int bit = (b >> j) & 1;
            encode_bit(low, high, pending, ob, nb, out, oi, tree[ctx], bit);
            ctx = (ctx << 1) | bit;
        }
    }
    encode_bit(low, high, pending, ob, nb, out, oi, flag_prob, 0);
    pending++;
    if (low < V5_FIRST_QTR) { put_bit(0, ob, nb, out, oi); while (pending > 0) { put_bit(1, ob, nb, out, oi); pending--; } }
    else                    { put_bit(1, ob, nb, out, oi); while (pending > 0) { put_bit(0, ob, nb, out, oi); pending--; } }
    if (nb > 0) out[oi++] = (v5_byte)(ob << (8 - nb));
    return oi;
}

int v5_encode(const v5_byte in[V5_MAX_IN], int n, v5_byte out[V5_MAX_OUT]) {
    static v5_byte cout[V5_KWAY][V5_CHUNK_CAP];
    int clen[V5_KWAY];
    int chunk = (n + V5_KWAY - 1) / V5_KWAY;

    for (int c = 0; c < V5_KWAY; c++) {
        int start = c * chunk;
        int len = n - start; if (len > chunk) len = chunk; if (len < 0) len = 0;
        clen[c] = encode_chunk(in + start, len, cout[c]);
    }

    int oi = 0;
    for (int c = 0; c < V5_KWAY; c++) {
        out[oi++] = (v5_byte)(clen[c] & 0xFF);
        out[oi++] = (v5_byte)((clen[c] >> 8) & 0xFF);
    }
    for (int c = 0; c < V5_KWAY; c++)
        for (int i = 0; i < clen[c]; i++) out[oi++] = cout[c][i];
    return oi;
}

/* --- decoder, from best_hls/arith5_test.cpp --- */
static long g_bit; static const v5_byte *g_in; static int g_len;
static inline uint32_t gb() {
    int bi = (int)(g_bit >> 3), off = 7 - (int)(g_bit & 7); g_bit++;
    return (uint32_t)((bi < g_len) ? ((g_in[bi] >> off) & 1) : 0);
}
static inline int dbit(uint32_t &low, uint32_t &high, uint32_t &code, uint32_t &prob) {
    uint32_t range = high - low + 1, split = (range * prob) >> V5_PROB_BITS; int bit;
    if ((code - low) < split) { bit = 0; high = low + split - 1; } else { bit = 1; low = low + split; }
    for (;;) {
        if (high < V5_HALF) {}
        else if (low >= V5_HALF) { code -= V5_HALF; low -= V5_HALF; high -= V5_HALF; }
        else if (low >= V5_FIRST_QTR && high < V5_THIRD_QTR) { code -= V5_FIRST_QTR; low -= V5_FIRST_QTR; high -= V5_FIRST_QTR; }
        else break;
        low = (low << 1) & V5_TOP_VALUE; high = ((high << 1) | 1) & V5_TOP_VALUE;
        code = ((code << 1) | gb()) & V5_TOP_VALUE;
    }
    if (bit == 0) prob += (V5_PROB_TOTAL - prob) >> V5_MOVE_BITS; else prob -= prob >> V5_MOVE_BITS;
    return bit;
}
static int decode_chunk(const v5_byte *in, int len, v5_byte *out) {
    g_in = in; g_len = len; g_bit = 0;
    uint32_t flag = V5_PROB_INIT, tree[V5_NTREE];
    for (int i = 0; i < V5_NTREE; i++) tree[i] = V5_PROB_INIT;
    uint32_t low = 0, high = V5_TOP_VALUE, code = 0;
    for (int i = 0; i < V5_CODE_BITS; i++) code = (code << 1) | gb();
    int on = 0;
    for (;;) {
        if (!dbit(low, high, code, flag)) break;
        int ctx = 1, b = 0;
        for (int j = 7; j >= 0; j--) { int bit = dbit(low, high, code, tree[ctx]); b = (b << 1) | bit; ctx = (ctx << 1) | bit; }
        out[on++] = (v5_byte)b;
    }
    return on;
}
int v5_decode(const v5_byte *comp, int comp_len, v5_byte *out) {
    (void)comp_len;
    int clen[V5_KWAY], off = 2 * V5_KWAY, on = 0;
    for (int c = 0; c < V5_KWAY; c++) clen[c] = comp[2 * c] | (comp[2 * c + 1] << 8);
    for (int c = 0; c < V5_KWAY; c++) { on += decode_chunk(comp + off, clen[c], out + on); off += clen[c]; }
    return on;
}
