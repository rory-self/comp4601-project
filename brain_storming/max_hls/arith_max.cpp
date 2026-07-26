/*
 * Area-lean, massively replicated arithmetic encoder.
 *
 * Each lane uses one adaptive probability per bit position rather than a
 * 255-node prefix tree.  This deliberately exchanges some compression ratio
 * for much lower lane area and reset latency, allowing more independent lanes.
 */
#include "arith_max.h"

#ifndef KWAY
#define KWAY 32
#endif

#define CHUNK_CAP (MAX_IN / KWAY + 64)

static inline void put_bit_max(unsigned bit, uint8_t &ob, uint8_t &nb,
                               byte_t *out, uint16_t &oi) {
    ob = (uint8_t)((ob << 1) | (bit & 1u));
    nb++;
    if (nb == 8) {
        out[oi++] = ob;
        ob = 0;
        nb = 0;
    }
}

static inline void encode_bit_max(
    uint16_t &low, uint16_t &high, uint16_t &pending,
    uint8_t &ob, uint8_t &nb, byte_t *out, uint16_t &oi,
    uint16_t &prob, unsigned bit) {

    uint32_t range = (uint32_t)high - (uint32_t)low + 1u;
    uint32_t split = (range * (uint32_t)prob) >> PROB_BITS;

    if (bit == 0) {
        high = (uint16_t)((uint32_t)low + split - 1u);
    } else {
        low = (uint16_t)((uint32_t)low + split);
    }

Renorm:
    for (int r = 0; r < 2 * CODE_BITS; r++) {
        if (high < HALF) {
            put_bit_max(0, ob, nb, out, oi);
            while (pending != 0) {
                put_bit_max(1, ob, nb, out, oi);
                pending--;
            }
        } else if (low >= HALF) {
            put_bit_max(1, ob, nb, out, oi);
            while (pending != 0) {
                put_bit_max(0, ob, nb, out, oi);
                pending--;
            }
            low = (uint16_t)(low - HALF);
            high = (uint16_t)(high - HALF);
        } else if (low >= FIRST_QTR && high < THIRD_QTR) {
            pending++;
            low = (uint16_t)(low - FIRST_QTR);
            high = (uint16_t)(high - FIRST_QTR);
        } else {
            break;
        }
        low = (uint16_t)(low << 1);
        high = (uint16_t)((high << 1) | 1u);
    }

    if (bit == 0) {
        prob = (uint16_t)(prob + ((PROB_TOTAL - prob) >> MOVE_BITS));
    } else {
        prob = (uint16_t)(prob - (prob >> MOVE_BITS));
    }
}

static int encode_chunk_max(const byte_t *in, int n, byte_t *out) {
    uint16_t flag_prob = PROB_INIT;
    uint16_t bit_prob[8];
Init:
    for (int i = 0; i < 8; i++) bit_prob[i] = PROB_INIT;

    uint8_t ob = 0, nb = 0;
    uint16_t oi = 0;
    uint16_t low = 0, high = TOP_VALUE, pending = 0;

Symbols:
    for (int k = 0; k < n; k++) {
        encode_bit_max(low, high, pending, ob, nb, out, oi, flag_prob, 1);
    Bits:
        for (int j = 7; j >= 0; j--) {
            unsigned bit = ((unsigned)in[k] >> j) & 1u;
            encode_bit_max(low, high, pending, ob, nb, out, oi,
                           bit_prob[7 - j], bit);
        }
    }
    encode_bit_max(low, high, pending, ob, nb, out, oi, flag_prob, 0);

    pending++;
    if (low < FIRST_QTR) {
        put_bit_max(0, ob, nb, out, oi);
        while (pending != 0) {
            put_bit_max(1, ob, nb, out, oi);
            pending--;
        }
    } else {
        put_bit_max(1, ob, nb, out, oi);
        while (pending != 0) {
            put_bit_max(0, ob, nb, out, oi);
            pending--;
        }
    }
    if (nb != 0) out[oi++] = (byte_t)(ob << (8 - nb));
    return oi;
}

// Layout: K little-endian uint16 lengths, followed by K coded chunks.
int arith_encode_max(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif

    static byte_t input_bank[KWAY][CHUNK_CAP];
    static byte_t output_bank[KWAY][CHUNK_CAP];
    int input_len[KWAY];
    int coded_len[KWAY];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=input_bank dim=1 complete
#pragma HLS ARRAY_PARTITION variable=output_bank dim=1 complete
#pragma HLS ARRAY_PARTITION variable=input_len complete
#pragma HLS ARRAY_PARTITION variable=coded_len complete
#endif

    int chunk = (n + KWAY - 1) / KWAY;
Load:
    for (int c = 0; c < KWAY; c++) {
        int start = c * chunk;
        int len = n - start;
        if (len > chunk) len = chunk;
        if (len < 0) len = 0;
        input_len[c] = len;
        for (int i = 0; i < len; i++) input_bank[c][i] = in[start + i];
    }

Code:
    for (int c = 0; c < KWAY; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
        coded_len[c] = encode_chunk_max(input_bank[c], input_len[c],
                                        output_bank[c]);
    }

    int oi = 0;
Header:
    for (int c = 0; c < KWAY; c++) {
        out[oi++] = (byte_t)(coded_len[c] & 0xff);
        out[oi++] = (byte_t)((coded_len[c] >> 8) & 0xff);
    }
Store:
    for (int c = 0; c < KWAY; c++) {
        for (int i = 0; i < coded_len[c]; i++) out[oi++] = output_bank[c][i];
    }
    return oi;
}

