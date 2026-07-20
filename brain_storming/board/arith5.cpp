/*
 * V5: K-way multi-stream parallel arithmetic coder.
 * Split the input into K independent chunks and code them with K concurrent
 * coder instances. Aggregate throughput ~ K x single-stream (the standard
 * way to accelerate arithmetic coding, since a single stream is sequential).
 */
#include "arith3.h"   // reuses encode_bit / put_bit / model params

#ifndef KWAY
#define KWAY 4
#endif
#define CHUNK_CAP  (MAX_IN / KWAY + 64)   // per-chunk output capacity

// Encode one independent chunk into a local buffer; returns byte length.
static int encode_chunk(const byte_t *in, int n, byte_t *out) {
    uint32_t flag_prob = PROB_INIT, tree[NTREE];
    for (int i = 0; i < NTREE; i++) tree[i] = PROB_INIT;
    uint32_t ob = 0; int nb = 0, oi = 0;
    uint32_t low = 0, high = TOP_VALUE, pending = 0;

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
    if (low < FIRST_QTR) { put_bit(0, ob, nb, out, oi); while (pending > 0) { put_bit(1, ob, nb, out, oi); pending--; } }
    else                 { put_bit(1, ob, nb, out, oi); while (pending > 0) { put_bit(0, ob, nb, out, oi); pending--; } }
    if (nb > 0) out[oi++] = (byte_t)(ob << (8 - nb));
    return oi;
}

// Output layout: [K x uint16 chunk-length header][chunk0 bytes][chunk1]...[chunk K-1]
int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]) {
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1

    // Burst input into K separate banks so the K coders can read concurrently.
    static byte_t buf[KWAY][CHUNK_CAP];
#pragma HLS ARRAY_PARTITION variable=buf dim=1 complete
    static byte_t cout[KWAY][CHUNK_CAP];
#pragma HLS ARRAY_PARTITION variable=cout dim=1 complete
    int clen[KWAY];
#pragma HLS ARRAY_PARTITION variable=clen dim=1 complete

    int chunk = (n + KWAY - 1) / KWAY;
Split:
    for (int c = 0; c < KWAY; c++) {
        int start = c * chunk;
        int len = n - start; if (len > chunk) len = chunk; if (len < 0) len = 0;
        for (int i = 0; i < len; i++) buf[c][i] = in[start + i];
        clen[c] = -len - 1;   // stash the input length (encoded) temporarily
    }

    // K concurrent coder instances.
Encode_K:
    for (int c = 0; c < KWAY; c++) {
#pragma HLS UNROLL
        int len = -(clen[c]) - 1;
        clen[c] = encode_chunk(buf[c], len, cout[c]);
    }

    // Assemble: header of K 16-bit lengths, then chunk bytes.
    int oi = 0;
Header:
    for (int c = 0; c < KWAY; c++) {
        out[oi++] = (byte_t)(clen[c] & 0xFF);
        out[oi++] = (byte_t)((clen[c] >> 8) & 0xFF);
    }
Concat:
    for (int c = 0; c < KWAY; c++)
        for (int i = 0; i < clen[c]; i++) out[oi++] = cout[c][i];

    return oi;
}
