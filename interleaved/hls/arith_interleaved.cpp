/*
 * C-slow/time-interleaved adaptive arithmetic encoder.
 *
 * One shared datapath visits LANES independent coder states round-robin.  A
 * given lane is revisited only after LANES iterations, turning the interval
 * recurrence from dependence distance 1 into dependence distance LANES.
 */
#include "arith_interleaved.h"

#ifndef LANES
#define LANES 16
#endif

#ifndef GROUPS
#define GROUPS 1
#endif

#if (LANES & (LANES - 1)) != 0
#error LANES must be a power of two
#endif

#if (GROUPS & (GROUPS - 1)) != 0
#error GROUPS must be a power of two
#endif

#if GROUPS > LANES
#error GROUPS cannot exceed LANES
#endif

#define CONTEXTS_PER_GROUP (LANES / GROUPS)

#if LANES == 2
#define LANE_BITS 1
#elif LANES == 4
#define LANE_BITS 2
#elif LANES == 8
#define LANE_BITS 3
#elif LANES == 16
#define LANE_BITS 4
#elif LANES == 32
#define LANE_BITS 5
#else
#error Supported LANES values are 2, 4, 8, 16, and 32
#endif

#define CHUNK_CAP (MAX_IN / LANES + 64)
#define CHUNK_WORD_CAP ((CHUNK_CAP + 7) / 8)
#define MAX_STEPS_PER_LANE (9 * CHUNK_CAP * 20 + 128)

enum Phase : uint8_t { FEED = 0, RENORM = 1, FLUSH = 2, DONE = 3 };

static inline void emit_one(unsigned bit, uint8_t &obyte, uint8_t &nbits,
                            byte_t *out, uint16_t &oidx) {
    obyte = (uint8_t)((obyte << 1) | (bit & 1u));
    nbits++;
    if (nbits == 8) {
        out[oidx++] = obyte;
        obyte = 0;
        nbits = 0;
    }
}

int arith_encode_interleaved(const input_t in[INPUT_DEPTH], int n,
                             byte_t out[MAX_OUT]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif

#ifdef WIDE_INPUT
    static uint64_t input_bank[LANES][CHUNK_WORD_CAP];
#else
    static byte_t input_bank[LANES][CHUNK_CAP];
#endif
    static byte_t output_bank[LANES][CHUNK_CAP];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=input_bank dim=1 complete
#pragma HLS ARRAY_PARTITION variable=output_bank dim=1 complete
#endif
    uint16_t input_len[LANES], output_len[LANES];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=input_len complete
#pragma HLS ARRAY_PARTITION variable=output_len complete
#endif

    int chunk = (n + LANES - 1) / LANES;
Load:
    for (int c = 0; c < LANES; c++) {
        int start = c * chunk;
        int len = n - start;
        if (len > chunk) len = chunk;
        if (len < 0) len = 0;
        input_len[c] = (uint16_t)len;
#ifdef WIDE_INPUT
        int words = (len + 7) >> 3;
        for (int w = 0; w < words; w++) {
            uint64_t packed = 0;
        PackInput:
            for (int b = 0; b < 8; b++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
                int local = (w << 3) + b;
                int global = start + local;
                byte_t value = 0;
                if (local < len) {
                    uint64_t source = in[global >> 3];
                    value = (byte_t)(source >> ((global & 7) << 3));
                }
                packed |= (uint64_t)value << (b << 3);
            }
            input_bank[c][w] = packed;
        }
#else
        for (int i = 0; i < len; i++) input_bank[c][i] = in[start + i];
#endif
    }

    uint16_t low[LANES], high[LANES], pending[LANES];
    uint16_t flag_prob[LANES], tree[LANES][NTREE];
    uint16_t byte_index[LANES], out_index[LANES], pending_out[LANES];
    uint16_t context[LANES];
    uint8_t stage[LANES], current_byte[LANES];
    uint8_t out_byte[LANES], out_bits[LANES], pending_value[LANES];
    uint8_t eof_done[LANES], flush_started[LANES], phase[LANES];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=low complete
#pragma HLS ARRAY_PARTITION variable=high complete
#pragma HLS ARRAY_PARTITION variable=pending complete
#pragma HLS ARRAY_PARTITION variable=flag_prob complete
#pragma HLS ARRAY_PARTITION variable=tree dim=1 complete
#pragma HLS ARRAY_PARTITION variable=byte_index complete
#pragma HLS ARRAY_PARTITION variable=out_index complete
#pragma HLS ARRAY_PARTITION variable=pending_out complete
#pragma HLS ARRAY_PARTITION variable=context complete
#pragma HLS ARRAY_PARTITION variable=stage complete
#pragma HLS ARRAY_PARTITION variable=current_byte complete
#pragma HLS ARRAY_PARTITION variable=out_byte complete
#pragma HLS ARRAY_PARTITION variable=out_bits complete
#pragma HLS ARRAY_PARTITION variable=pending_value complete
#pragma HLS ARRAY_PARTITION variable=eof_done complete
#pragma HLS ARRAY_PARTITION variable=flush_started complete
#pragma HLS ARRAY_PARTITION variable=phase complete
#endif

InitLanes:
    for (int c = 0; c < LANES; c++) {
        low[c] = 0;
        high[c] = TOP_VALUE;
        pending[c] = 0;
        flag_prob[c] = PROB_INIT;
        byte_index[c] = 0;
        out_index[c] = 0;
        pending_out[c] = 0;
        context[c] = 1;
        stage[c] = 0;
        current_byte[c] = 0;
        out_byte[c] = 0;
        out_bits[c] = 0;
        pending_value[c] = 0;
        eof_done[c] = 0;
        flush_started[c] = 0;
        phase[c] = FEED;
        output_len[c] = 0;
    }

InitModels:
    for (int i = 0; i < NTREE; i++) {
        for (int c = 0; c < LANES; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            tree[c][i] = PROB_INIT;
        }
    }

    uint8_t done_count[GROUPS];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=done_count complete
#endif
InitDone:
    for (int g = 0; g < GROUPS; g++) done_count[g] = 0;

    /*
     * One bounded micro-operation for one lane per iteration.  Explicit II=1
     * is the central experiment: LANES cycles separate dependent operations
     * belonging to the same arithmetic stream.
     */
Interleave:
    for (int iter = 0; iter < CONTEXTS_PER_GROUP * MAX_STEPS_PER_LANE; iter++) {
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
        bool all_done = true;
    CheckDone:
        for (int g = 0; g < GROUPS; g++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            if (done_count[g] != CONTEXTS_PER_GROUP) all_done = false;
        }
        if (all_done) break;

    Groups:
        for (int g = 0; g < GROUPS; g++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            unsigned slot = (unsigned)iter & (CONTEXTS_PER_GROUP - 1);
            unsigned c = g * CONTEXTS_PER_GROUP + slot;
            uint8_t p = phase[c];

            if (p == FEED) {
            uint16_t prob;
            unsigned bit;
            uint16_t tree_context = 0;
            bool use_tree = false;

            if (byte_index[c] < input_len[c]) {
                if (stage[c] == 0) {
                    prob = flag_prob[c];
                    bit = 1;
#ifdef WIDE_INPUT
                    uint16_t source_index = byte_index[c];
                    uint64_t source_word = input_bank[c][source_index >> 3];
                    current_byte[c] =
                        (byte_t)(source_word >> ((source_index & 7) << 3));
#else
                    current_byte[c] = input_bank[c][byte_index[c]];
#endif
                    context[c] = 1;
                    stage[c] = 1;
                } else {
                    int shift = 8 - stage[c];
                    bit = ((unsigned)current_byte[c] >> shift) & 1u;
                    tree_context = context[c];
                    prob = tree[c][tree_context];
                    use_tree = true;
                    context[c] = (uint16_t)((context[c] << 1) | bit);
                    stage[c]++;
                    if (stage[c] == 9) {
                        byte_index[c]++;
                        stage[c] = 0;
                    }
                }
            } else if (!eof_done[c]) {
                prob = flag_prob[c];
                bit = 0;
                eof_done[c] = 1;
            } else {
                phase[c] = FLUSH;
                continue;
            }

            uint32_t range = (uint32_t)high[c] - low[c] + 1u;
            uint32_t split = (range * prob) >> PROB_BITS;
            if (bit == 0)
                high[c] = (uint16_t)((uint32_t)low[c] + split - 1u);
            else
                low[c] = (uint16_t)((uint32_t)low[c] + split);

            uint16_t next_prob =
                bit == 0
                    ? (uint16_t)(prob + ((PROB_TOTAL - prob) >> MOVE_BITS))
                    : (uint16_t)(prob - (prob >> MOVE_BITS));
            if (use_tree) tree[c][tree_context] = next_prob;
            else flag_prob[c] = next_prob;
            phase[c] = RENORM;
            } else if (p == RENORM) {
            if (pending_out[c] != 0) {
                emit_one(pending_value[c], out_byte[c], out_bits[c],
                         output_bank[c], out_index[c]);
                pending_out[c]--;
            } else if (high[c] < HALF) {
                emit_one(0, out_byte[c], out_bits[c], output_bank[c],
                         out_index[c]);
                pending_value[c] = 1;
                pending_out[c] = pending[c];
                pending[c] = 0;
                low[c] = (uint16_t)(low[c] << 1);
                high[c] = (uint16_t)((high[c] << 1) | 1u);
            } else if (low[c] >= HALF) {
                emit_one(1, out_byte[c], out_bits[c], output_bank[c],
                         out_index[c]);
                pending_value[c] = 0;
                pending_out[c] = pending[c];
                pending[c] = 0;
                low[c] = (uint16_t)(low[c] - HALF);
                high[c] = (uint16_t)(high[c] - HALF);
                low[c] = (uint16_t)(low[c] << 1);
                high[c] = (uint16_t)((high[c] << 1) | 1u);
            } else if (low[c] >= FIRST_QTR && high[c] < THIRD_QTR) {
                pending[c]++;
                low[c] = (uint16_t)(low[c] - FIRST_QTR);
                high[c] = (uint16_t)(high[c] - FIRST_QTR);
                low[c] = (uint16_t)(low[c] << 1);
                high[c] = (uint16_t)((high[c] << 1) | 1u);
            } else {
                phase[c] = FEED;
            }
            } else if (p == FLUSH) {
            if (!flush_started[c]) {
                pending[c]++;
                if (low[c] < FIRST_QTR) {
                    emit_one(0, out_byte[c], out_bits[c], output_bank[c],
                             out_index[c]);
                    pending_value[c] = 1;
                } else {
                    emit_one(1, out_byte[c], out_bits[c], output_bank[c],
                             out_index[c]);
                    pending_value[c] = 0;
                }
                pending_out[c] = pending[c];
                pending[c] = 0;
                flush_started[c] = 1;
            } else if (pending_out[c] != 0) {
                emit_one(pending_value[c], out_byte[c], out_bits[c],
                         output_bank[c], out_index[c]);
                pending_out[c]--;
            } else {
                if (out_bits[c] != 0) {
                    output_bank[c][out_index[c]++] =
                        (byte_t)(out_byte[c] << (8 - out_bits[c]));
                    out_bits[c] = 0;
                }
                output_len[c] = out_index[c];
                phase[c] = DONE;
                done_count[g]++;
            }
        }
        }
    }

    int oi = 0;
Header:
    for (int c = 0; c < LANES; c++) {
        out[oi++] = (byte_t)(output_len[c] & 0xff);
        out[oi++] = (byte_t)(output_len[c] >> 8);
    }
Store:
    for (int c = 0; c < LANES; c++)
        for (int i = 0; i < output_len[c]; i++) out[oi++] = output_bank[c][i];

    return oi;
}
