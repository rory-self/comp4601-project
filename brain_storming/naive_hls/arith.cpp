/*
 * Baseline adaptive arithmetic encoder (Witten-Neal-Cleary, 16-bit).
 * Correctness-first, unoptimised - this is the starting point for HLS
 * design-space exploration (the sequential interval-update recurrence and
 * the divide are expected to dominate).
 */
#include "arith.h"

void arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int *out_len) {
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1

    // Adaptive frequency model (start uniform: every symbol freq 1).
    uint32_t freq[NSYM];
    uint32_t cum[NSYM + 1];
Init_Model:
    for (int i = 0; i < NSYM; i++) freq[i] = 1;

    // Bit-packing output state.
    uint32_t out_byte = 0;
    int      out_nbits = 0;
    int      out_idx = 0;

    // Arithmetic interval state.
    uint32_t low = 0, high = TOP_VALUE, pending = 0;

Symbol_Loop:
    for (int k = 0; k <= n; k++) {          // n input symbols, then EOF
        int s = (k < n) ? (int)in[k] : EOF_SYM;

        // Cumulative frequencies (naive O(NSYM) build).
        uint32_t total = 0;
    Cum_Loop:
        for (int i = 0; i < NSYM; i++) { cum[i] = total; total += freq[i]; }
        cum[NSYM] = total;

        uint32_t cumLow  = cum[s];
        uint32_t cumHigh = cum[s + 1];

        // Interval update (the sequential recurrence + divide).
        uint32_t range = high - low + 1;
        high = low + (uint32_t)(((uint64_t)range * cumHigh) / total) - 1;
        low  = low + (uint32_t)(((uint64_t)range * cumLow ) / total);

        // Renormalisation.
    Renorm_Loop:
        for (;;) {
            if (high < HALF) {
                // emit 0 then pending 1s
                out_byte = (out_byte << 1); out_nbits++;
                if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
                while (pending > 0) {
                    out_byte = (out_byte << 1) | 1; out_nbits++;
                    if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
                    pending--;
                }
            } else if (low >= HALF) {
                out_byte = (out_byte << 1) | 1; out_nbits++;
                if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
                while (pending > 0) {
                    out_byte = (out_byte << 1); out_nbits++;
                    if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
                    pending--;
                }
                low -= HALF; high -= HALF;
            } else if (low >= FIRST_QTR && high < THIRD_QTR) {
                pending++; low -= FIRST_QTR; high -= FIRST_QTR;
            } else {
                break;
            }
            low  = (low << 1) & TOP_VALUE;
            high = ((high << 1) | 1) & TOP_VALUE;
        }

        // Adaptive update.
        freq[s]++;
        if (total + 1 >= MAX_FREQ) {
        Rescale_Loop:
            for (int i = 0; i < NSYM; i++) freq[i] = (freq[i] + 1) >> 1;
        }
    }

    // Flush: 2 disambiguating bits + pending.
    pending++;
    if (low < FIRST_QTR) {
        out_byte = (out_byte << 1); out_nbits++;
        if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
        while (pending > 0) {
            out_byte = (out_byte << 1) | 1; out_nbits++;
            if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
            pending--;
        }
    } else {
        out_byte = (out_byte << 1) | 1; out_nbits++;
        if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
        while (pending > 0) {
            out_byte = (out_byte << 1); out_nbits++;
            if (out_nbits == 8) { out[out_idx++] = out_byte; out_byte = 0; out_nbits = 0; }
            pending--;
        }
    }
    // Pad final partial byte (MSB-first).
    if (out_nbits > 0) { out[out_idx++] = out_byte << (8 - out_nbits); }

    *out_len = out_idx;
}
