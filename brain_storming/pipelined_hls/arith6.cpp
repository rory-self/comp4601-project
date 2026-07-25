/*
 * V6 (iteration 2): PIPELINED single-stream coder + K-way replication.
 *
 * Why V5 could not be pipelined: encode_bit() nests a variable-length
 * renormalisation loop (0..2*CODE_BITS emitted bits) inside every coding
 * step, so the loop body has no fixed shape and Vitis cannot give the
 * coding loop an initiation interval (a forced PIPELINE pragma does not
 * converge). Each coded bit therefore costs ~9 cycles.
 *
 * V6 restructures the coder into a FLAT STATE MACHINE: a single loop where
 * every iteration performs exactly ONE small unit of work --
 *   S_CODE   : one interval update (codes one binary decision), or
 *   S_RENORM : one renormalisation step E1/E2/E3 (emits at most one bit), or
 *   S_DRAIN  : emits one deferred "pending" underflow bit, or
 *   S_FLUSH  : the final flush decision.
 * The body now has a small fixed shape, so the loop pipelines at II=1:
 * one unit of work per clock. Expected cost per byte symbol is roughly
 *   9 (coded bits) + (output bits) + (E3 underflow steps)  ~ 11-14 cycles,
 * vs ~84 cycles/symbol for V5's single stream -- a ~6-8x single-stream
 * speedup that MULTIPLIES with the existing K-way replication.
 *
 * The output is BIT-EXACT with V5 (asserted by arith6_test.cpp), so the V5
 * decoder, the board host and the demo all work unchanged.
 */
#include "arith3.h"

#ifndef KWAY
#define KWAY 4
#endif
#define CHUNK_CAP  (MAX_IN / KWAY + 64)   // per-chunk output capacity

// Trip-count hints for the flat loop (reporting only): ~13 work units per
// input byte on typical data, bounded by full renorms on every coded bit.
#define FLAT_AVG (13 * CHUNK_CAP)
#define FLAT_MAX (40 * CHUNK_CAP)

// Flat-coder states.
#define S_CODE    0   // code one binary decision (interval update)
#define S_RENORM  1   // one renormalisation step (E1/E2/E3)
#define S_DRAIN   2   // emit one pending underflow bit
#define S_FLUSH   3   // final flush decision
#define S_DONE    4

// After an interval update or a renorm step, decide the next state from the
// (new) interval. Cheap: HALF/QTR comparisons only look at the top two bits.
static inline int next_state(uint32_t low, uint32_t high, bool eof_coded) {
    bool renorm = (high < HALF) || (low >= HALF)
               || (low >= FIRST_QTR && high < THIRD_QTR);
    return renorm ? S_RENORM : (eof_coded ? S_FLUSH : S_CODE);
}

#ifndef __SYNTHESIS__
// Work-unit counters (host-only): at II=1 one flat-loop iteration ~= one
// clock cycle, so these predict the hardware cycle count from a SW run.
long g_flat_units = 0;      // total units across all chunks
long g_flat_units_max = 0;  // worst chunk (= parallel critical path)
#endif

// Encode one independent chunk into a local buffer; returns byte length.
// Bit-exact re-scheduling of V5's encode_chunk().
static int encode_chunk_flat(const byte_t *in, int n, byte_t *out) {
    uint32_t flag_prob = PROB_INIT;
    uint32_t tree[NTREE];
#pragma HLS ARRAY_PARTITION variable=tree complete
Init:
    for (int i = 0; i < NTREE; i++) {
#pragma HLS UNROLL
        tree[i] = PROB_INIT;
    }

    uint32_t ob = 0; int nb = 0, oi = 0;             // output bit-packer
    uint32_t low = 0, high = TOP_VALUE, pending = 0; // interval state

    // Position in the coded-bit sequence: p==0 -> "more symbols" flag,
    // p==1..8 -> data bit (MSB first) of the current byte b.
    int k = 0, p = 0, ctx = 1;
    byte_t b = 0;

    int state = S_CODE;
    int resume = S_RENORM;      // where S_DRAIN returns to
    uint32_t drain_bit = 0;
    bool eof_coded = false;     // the final flag(0) has been coded

#ifndef __SYNTHESIS__
    long units = 0;
#endif
Flat:
    while (state != S_DONE) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=8 max=FLAT_MAX avg=FLAT_AVG
#ifndef __SYNTHESIS__
        units++;
#endif
        if (state == S_CODE) {
            // ---- one interval update (codes one binary decision) ----
            bool is_flag = (p == 0);
            int bit; uint32_t prob;
            if (is_flag) { bit = (k < n) ? 1 : 0;      prob = flag_prob;  }
            else         { bit = (b >> (8 - p)) & 1;   prob = tree[ctx]; }

            uint32_t range = high - low + 1;
            uint32_t split = (range * prob) >> PROB_BITS;   // shift, no divide
            if (bit == 0) high = low + split - 1;
            else          low  = low + split;

            // Adaptive model update (independent of renormalisation).
            uint32_t nprob = (bit == 0) ? prob + ((PROB_TOTAL - prob) >> MOVE_BITS)
                                        : prob - (prob >> MOVE_BITS);
            if (is_flag) flag_prob = nprob; else tree[ctx] = nprob;

            // Advance the bit sequence.
            bool eof_now = eof_coded;
            if (is_flag) {
                if (k < n) { b = in[k]; ctx = 1; p = 1; }
                else       { eof_now = true; }
            } else {
                ctx = (ctx << 1) | bit;
                if (p == 8) { k++; p = 0; } else p++;
            }
            eof_coded = eof_now;
            state = next_state(low, high, eof_now);

        } else if (state == S_RENORM) {
            // ---- one renormalisation step (entered only when needed) ----
            if (high < HALF) {                                   // E1: emit 0
                put_bit(0, ob, nb, out, oi);
                low  = (low << 1) & TOP_VALUE;
                high = ((high << 1) | 1) & TOP_VALUE;
                int nxt = next_state(low, high, eof_coded);
                if (pending > 0) { drain_bit = 1; resume = nxt; state = S_DRAIN; }
                else state = nxt;
            } else if (low >= HALF) {                            // E2: emit 1
                put_bit(1, ob, nb, out, oi);
                low  = ((low  - HALF) << 1) & TOP_VALUE;
                high = (((high - HALF) << 1) | 1) & TOP_VALUE;
                int nxt = next_state(low, high, eof_coded);
                if (pending > 0) { drain_bit = 0; resume = nxt; state = S_DRAIN; }
                else state = nxt;
            } else {                                             // E3: underflow
                pending++;
                low  = ((low  - FIRST_QTR) << 1) & TOP_VALUE;
                high = (((high - FIRST_QTR) << 1) | 1) & TOP_VALUE;
                state = next_state(low, high, eof_coded);
            }

        } else if (state == S_DRAIN) {
            // ---- emit one deferred underflow bit ----
            put_bit(drain_bit, ob, nb, out, oi);
            pending--;
            if (pending == 0) state = resume;

        } else { // S_FLUSH
            // ---- final flush: one disambiguating bit + pending bits ----
            pending++;
            if (low < FIRST_QTR) { put_bit(0, ob, nb, out, oi); drain_bit = 1; }
            else                 { put_bit(1, ob, nb, out, oi); drain_bit = 0; }
            resume = S_DONE; state = S_DRAIN;
        }
    }

#ifndef __SYNTHESIS__
    g_flat_units += units;
    if (units > g_flat_units_max) g_flat_units_max = units;
#endif
    if (nb > 0) out[oi++] = (byte_t)(ob << (8 - nb));   // pad final byte
    return oi;
}

// Output layout: [K x uint16 chunk-length header][chunk0 bytes][chunk1]...
// Identical to V5's arith_encode, but each coder is the pipelined flat coder.
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

    // K concurrent pipelined coder instances.
Encode_K:
    for (int c = 0; c < KWAY; c++) {
#pragma HLS UNROLL
        int len = -(clen[c]) - 1;
        clen[c] = encode_chunk_flat(buf[c], len, cout[c]);
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
