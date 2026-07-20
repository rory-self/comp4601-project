#ifndef ARITH3_H_
#define ARITH3_H_
#include <stdint.h>

// V3: adaptive BINARY range coder (LZMA-style bit model).
// Shift-based interval update (no divide); byte alphabet via an 8-level bit-tree.
#define MAX_IN    4096
#define MAX_OUT   8192

#define CODE_BITS   16
#define TOP_VALUE   ((uint32_t)((1u << CODE_BITS) - 1))
#define FIRST_QTR   ((TOP_VALUE + 1) / 4)
#define HALF        (2 * FIRST_QTR)
#define THIRD_QTR   (3 * FIRST_QTR)

#define PROB_BITS   12
#define PROB_TOTAL  (1u << PROB_BITS)       // 4096, a power of 2 -> shift, not divide
#define PROB_INIT   (PROB_TOTAL / 2)        // 2048
#define MOVE_BITS   5

#define NTREE       256                     // bit-tree contexts (index 1..255)

typedef unsigned char byte_t;

static inline void put_bit(int bit, uint32_t &ob, int &nb, byte_t *out, int &oi) {
    ob = (ob << 1) | (bit & 1); nb++;
    if (nb == 8) { out[oi++] = (byte_t)ob; ob = 0; nb = 0; }
}

// Encode one binary decision. prob = P(bit==0), scaled to PROB_TOTAL.
static inline void encode_bit(uint32_t &low, uint32_t &high, uint32_t &pending,
                              uint32_t &ob, int &nb, byte_t *out, int &oi,
                              uint32_t &prob, int bit) {
    uint32_t range = high - low + 1;
    uint32_t split = (range * prob) >> PROB_BITS;   // shift, no divide
    if (bit == 0) high = low + split - 1;
    else          low  = low + split;

Renorm:
    for (int r = 0; r < 2 * CODE_BITS; r++) {
        if (high < HALF) {
            put_bit(0, ob, nb, out, oi);
            while (pending > 0) { put_bit(1, ob, nb, out, oi); pending--; }
        } else if (low >= HALF) {
            put_bit(1, ob, nb, out, oi);
            while (pending > 0) { put_bit(0, ob, nb, out, oi); pending--; }
            low -= HALF; high -= HALF;
        } else if (low >= FIRST_QTR && high < THIRD_QTR) {
            pending++; low -= FIRST_QTR; high -= FIRST_QTR;
        } else break;
        low  = (low << 1) & TOP_VALUE;
        high = ((high << 1) | 1) & TOP_VALUE;
    }

    if (bit == 0) prob += (PROB_TOTAL - prob) >> MOVE_BITS;
    else          prob -= prob >> MOVE_BITS;
}

int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);

#endif
