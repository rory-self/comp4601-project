/*
 * C-slow/time-interleaved adaptive arithmetic encoder, width-tightened (v2).
 *
 * Identical dataflow to arith_interleaved.cpp, but every recurrence-critical
 * state variable is an exact-width ap_uint.  The interval-update chain that
 * closes the loop-carried dependency (sub -> multiply -> add) therefore uses a
 * 17-bit datapath and a 17x13 multiplier instead of 32-bit arithmetic and a
 * 17x16 multiplier, shortening the critical path so the highest-GROUPS
 * configuration (recurrence distance = LANES/GROUPS) can close timing faster.
 *
 * ap_int.h compiles under both g++ and Vitis HLS, so this single source is what
 * the software test runs *and* what is synthesised -- no divergent arithmetic.
 */
#include "ap_int.h"
#include "arith_interleaved_v2.h"

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
#define CHUNK_CAP (MAX_IN / LANES + 64)
#define MAX_STEPS_PER_LANE (9 * CHUNK_CAP * 20 + 128)

// Exact-width types for the arithmetic-coder state.
typedef ap_uint<16> code_t;   // low / high              (CODE_BITS)
typedef ap_uint<17> range_t;  // range / split           (up to 65536)
typedef ap_uint<13> prob_t;   // probabilities           (0..4095)
typedef ap_uint<9>  ctx_t;    // bit-tree context index  (1..255, transient 511)
typedef ap_uint<16> idx_t;    // buffer indices / pending counts

enum Phase : uint8_t { FEED = 0, RENORM = 1, FLUSH = 2, DONE = 3 };

static inline void emit_one(unsigned bit, uint8_t &obyte, uint8_t &nbits,
                            byte_t *out, idx_t &oidx) {
    obyte = (uint8_t)((obyte << 1) | (bit & 1u));
    nbits++;
    if (nbits == 8) {
        out[oidx++] = obyte;
        obyte = 0;
        nbits = 0;
    }
}

int arith_encode_interleaved(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif

    static byte_t input_bank[LANES][CHUNK_CAP];
    static byte_t output_bank[LANES][CHUNK_CAP];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=input_bank dim=1 complete
#pragma HLS ARRAY_PARTITION variable=output_bank dim=1 complete
#endif
    idx_t input_len[LANES], output_len[LANES];
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
        input_len[c] = (idx_t)len;
        for (int i = 0; i < len; i++) input_bank[c][i] = in[start + i];
    }

    code_t low[LANES], high[LANES];
    idx_t  pending[LANES];
    prob_t flag_prob[LANES], tree[LANES][NTREE];
    idx_t  byte_index[LANES], out_index[LANES], pending_out[LANES];
    ctx_t  context[LANES];
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
            prob_t prob;
            unsigned bit;
            ctx_t tree_context = 0;
            bool use_tree = false;

            if (byte_index[c] < input_len[c]) {
                if (stage[c] == 0) {
                    prob = flag_prob[c];
                    bit = 1;
                    current_byte[c] = input_bank[c][byte_index[c]];
                    context[c] = 1;
                    stage[c] = 1;
                } else {
                    int shift = 8 - stage[c];
                    bit = ((unsigned)current_byte[c] >> shift) & 1u;
                    tree_context = context[c];
                    prob = tree[c][tree_context];
                    use_tree = true;
                    context[c] = (ctx_t)((context[c] << 1) | bit);
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

            // Interval update: 17-bit sub, 17x13 multiply, 17-bit add.
            range_t range = (range_t)((range_t)high[c] - (range_t)low[c] + 1);
            ap_uint<30> prod = (ap_uint<30>)range * prob;
#if defined(__SYNTHESIS__) && defined(REG_MUL)
            // Register the DSP multiply so the interval-update recurrence spans
            // two pipeline stages (sub | mul | add), letting the short-distance
            // (high-GROUPS) configs still close II=1 at a higher clock.
#pragma HLS BIND_OP variable=prod op=mul impl=dsp latency=1
#endif
            range_t split = (range_t)(prod >> PROB_BITS);
            if (bit == 0)
                high[c] = (code_t)((range_t)low[c] + split - 1);
            else
                low[c] = (code_t)((range_t)low[c] + split);

            prob_t next_prob =
                bit == 0
                    ? (prob_t)(prob + ((PROB_TOTAL - prob) >> MOVE_BITS))
                    : (prob_t)(prob - (prob >> MOVE_BITS));
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
                low[c] = (code_t)(low[c] << 1);
                high[c] = (code_t)((high[c] << 1) | 1u);
            } else if (low[c] >= HALF) {
                emit_one(1, out_byte[c], out_bits[c], output_bank[c],
                         out_index[c]);
                pending_value[c] = 0;
                pending_out[c] = pending[c];
                pending[c] = 0;
                low[c] = (code_t)(low[c] - HALF);
                high[c] = (code_t)(high[c] - HALF);
                low[c] = (code_t)(low[c] << 1);
                high[c] = (code_t)((high[c] << 1) | 1u);
            } else if (low[c] >= FIRST_QTR && high[c] < THIRD_QTR) {
                pending[c]++;
                low[c] = (code_t)(low[c] - FIRST_QTR);
                high[c] = (code_t)(high[c] - FIRST_QTR);
                low[c] = (code_t)(low[c] << 1);
                high[c] = (code_t)((high[c] << 1) | 1u);
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
