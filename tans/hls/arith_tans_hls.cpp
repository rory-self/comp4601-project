/*
 * HLS static tANS encoder (the "tree method"): one whole byte per state
 * transition -- state = STATE[...] -- no multiply, no per-symbol model update.
 * The static table (baked in from a shared histogram) is the whole point: for
 * files that share a distribution you precompute it once and then just traverse.
 *
 * This measures the core throughput claim: ~1 byte per cycle per stream, vs the
 * M-coder's 8 bins (=1 byte) per 8 cycles. FSE-style reverse encode.
 */
#include "tans_table.h"

#define MAX_IN  4096
#define MAX_OUT 8192
typedef unsigned char byte_t;

int tans_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif
    static byte_t buf[MAX_IN];
    static byte_t obuf[MAX_OUT];
Load:
    for (int i = 0; i < n; i++) buf[i] = in[i];
    if (n == 0) return 0;

    // init state from the last symbol (FSE_initCState2)
    uint32_t s0 = buf[n - 1];
    uint32_t nb0 = (TANS_DNB[s0] + (1u << 15)) >> 16;
    uint32_t state = (nb0 << 16) - TANS_DNB[s0];
    state = TANS_STATE[(state >> nb0) + TANS_DFS[s0]];

    uint32_t acc = 0; int nacc = 0, oi = 0; long total_bits = 0;

Enc:
    for (int i = n - 2; i >= 0; i--) {
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
        uint32_t s  = buf[i];
        uint32_t nb = (state + TANS_DNB[s]) >> 16;              // bits to emit, <= L
        acc  = (acc << nb) | (state & ((1u << nb) - 1u));       // append low nb bits
        nacc += nb; total_bits += nb;
        // flush up to 2 bytes (nb<=12, nacc<8 before -> nacc<20): fixed, unrolled
        if (nacc >= 8) { obuf[oi++] = (byte_t)(acc >> (nacc - 8)); nacc -= 8; }
        if (nacc >= 8) { obuf[oi++] = (byte_t)(acc >> (nacc - 8)); nacc -= 8; }
        state = TANS_STATE[(state >> nb) + TANS_DFS[s]];        // the recurrence: one table read
    }
    // flush the final state (L bits) + tail
    acc = (acc << TANS_L) | (state & (TANS_SIZE - 1)); nacc += TANS_L; total_bits += TANS_L;
    if (nacc >= 8) { obuf[oi++] = (byte_t)(acc >> (nacc - 8)); nacc -= 8; }
    if (nacc >= 8) { obuf[oi++] = (byte_t)(acc >> (nacc - 8)); nacc -= 8; }
    if (nacc >  0) { obuf[oi++] = (byte_t)(acc << (8 - nacc)); nacc = 0; }

    // header: 2-byte payload bit count (little-endian), so the decoder can find
    // the true end of the MSB-first bit stream and read it backward (ANS is LIFO).
    out[0] = (byte_t)(total_bits & 0xff);
    out[1] = (byte_t)((total_bits >> 8) & 0xff);
Store:
    for (int i = 0; i < oi; i++) out[2 + i] = obuf[i];
    return oi + 2;
}
