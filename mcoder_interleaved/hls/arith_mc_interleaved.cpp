/*
 * Interleaved M-coder (iteration 3): Bryan's multiply-free CABAC core
 * (mcoder.h: mc_code_bin + closed-form renorm) run through OUR C-slow
 * time-interleaving harness.
 *
 * His replicated M-coder is stuck at ~167 MHz because each stream's low/range
 * recurrence has dependence distance 1. Here LANES independent coder states are
 * visited round-robin by GROUPS shared pipelines, so the same state is revisited
 * only every LANES/GROUPS cycles -- the recurrence gets slack, and the coding
 * loop can be pipelined at aggregate II=1 at a higher clock, still multiply-free
 * and with far fewer arithmetic datapaths than replicating LANES full coders.
 *
 * The variable-latency carry drain (Bryan splits it into a separate dataflow
 * packer stage) is folded inline here as one bit-emit per cycle, exactly like
 * the renorm phase of arith_interleaved_v2 -- so the whole engine stays in one
 * interleaved loop. Output format matches mc_encode, so mc_decode verifies it.
 */
#include "arith_mc_interleaved.h"

#ifndef LANES
#define LANES 16
#endif
#ifndef GROUPS
#define GROUPS 4
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
#define CHUNK_CAP          (MC_MAX_IN / LANES + 64)
#define MC_MAX_STEPS       (9 * CHUNK_CAP * 12 + 256)

enum Stage : uint8_t { S_BINS = 0, S_FLUSH2 = 1, S_PAD = 2, S_DONE = 3 };

static inline void mc_emit(uint8_t &acc, uint8_t &nacc, mc_byte *outb,
                           uint16_t &oi, unsigned bit) {
    acc = (uint8_t)((acc << 1) | (bit & 1u));
    nacc++;
    if (nacc == 8) { outb[oi++] = (mc_byte)acc; acc = 0; nacc = 0; }
}

int arith_mc_encode_interleaved(const mc_byte in[MC_MAX_IN], int n,
                                mc_byte out[MC_MAX_OUT]) {
#if defined(__SYNTHESIS__) && !defined(BOARD_WRAP)
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#endif

    static mc_byte input_bank[LANES][CHUNK_CAP];
    static mc_byte output_bank[LANES][CHUNK_CAP];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=input_bank dim=1 complete
#pragma HLS ARRAY_PARTITION variable=output_bank dim=1 complete
#endif
    uint16_t input_len[LANES], nbins[LANES], output_len[LANES];

    int chunk = (n + LANES - 1) / LANES;
Load:
    for (int c = 0; c < LANES; c++) {
        int start = c * chunk, len = n - start;
        if (len > chunk) len = chunk;
        if (len < 0) len = 0;
        input_len[c] = (uint16_t)len;
        nbins[c]     = (uint16_t)(len << 3);
        for (int i = 0; i < len; i++) input_bank[c][i] = in[start + i];
    }

    // --- per-lane coder + packer state ---
    uint32_t low[LANES], range[LANES];
    mc_ctx   tree[LANES][MC_NTREE];
    uint16_t ctx[LANES], bin_index[LANES], out_index[LANES];
    mc_byte  current_byte[LANES];
    uint8_t  acc[LANES], nacc[LANES], first[LANES], runbit[LANES], stage[LANES], rem[LANES];
    uint32_t outstanding[LANES], run[LANES], cls[LANES];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=low complete
#pragma HLS ARRAY_PARTITION variable=range complete
#pragma HLS ARRAY_PARTITION variable=tree dim=1 complete
#pragma HLS ARRAY_PARTITION variable=ctx complete
#pragma HLS ARRAY_PARTITION variable=bin_index complete
#pragma HLS ARRAY_PARTITION variable=out_index complete
#pragma HLS ARRAY_PARTITION variable=current_byte complete
#pragma HLS ARRAY_PARTITION variable=acc complete
#pragma HLS ARRAY_PARTITION variable=nacc complete
#pragma HLS ARRAY_PARTITION variable=first complete
#pragma HLS ARRAY_PARTITION variable=runbit complete
#pragma HLS ARRAY_PARTITION variable=stage complete
#pragma HLS ARRAY_PARTITION variable=rem complete
#pragma HLS ARRAY_PARTITION variable=outstanding complete
#pragma HLS ARRAY_PARTITION variable=run complete
#pragma HLS ARRAY_PARTITION variable=cls complete
#pragma HLS ARRAY_PARTITION variable=input_len complete
#pragma HLS ARRAY_PARTITION variable=nbins complete
#pragma HLS ARRAY_PARTITION variable=output_len complete
#endif

InitLanes:
    for (int c = 0; c < LANES; c++) {
        low[c] = 0; range[c] = MC_RANGE_INIT;
        ctx[c] = 1; bin_index[c] = 0; out_index[c] = 0; current_byte[c] = 0;
        acc[c] = 0; nacc[c] = 0; first[c] = 1; runbit[c] = 0;
        stage[c] = S_BINS; rem[c] = 0; outstanding[c] = 0; run[c] = 0; cls[c] = 0;
        output_len[c] = 0;
    }
InitModels:
    for (int i = 0; i < MC_NTREE; i++)
        for (int c = 0; c < LANES; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            tree[c][i] = MC_CTX_INIT;
        }

    uint8_t done_count[GROUPS];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=done_count complete
#endif
    for (int g = 0; g < GROUPS; g++) done_count[g] = 0;

Interleave:
    for (int iter = 0; iter < CONTEXTS_PER_GROUP * MC_MAX_STEPS; iter++) {
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
        bool all_done = true;
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
            uint8_t stg = stage[c];

            if (stg == S_DONE) {
                // idle
            } else if (run[c] != 0) {                 // drain a deferred-carry run
                mc_emit(acc[c], nacc[c], output_bank[c], out_index[c], runbit[c]);
                run[c]--;
            } else if (rem[c] != 0) {                 // one classification step
                unsigned cc = cls[c] & 3u; cls[c] >>= 2; rem[c]--;
                if (cc == MC_E3) {
                    outstanding[c]++;
                } else {
                    unsigned b = (cc == MC_E2) ? 1u : 0u;
                    if (first[c]) first[c] = 0;
                    else mc_emit(acc[c], nacc[c], output_bank[c], out_index[c], b);
                    runbit[c] = (uint8_t)(b ^ 1u);
                    run[c] = outstanding[c];
                    outstanding[c] = 0;
                }
            } else if (stg == S_BINS) {               // produce the next bin's token
                if (bin_index[c] < nbins[c]) {
                    unsigned j = (unsigned)bin_index[c] & 7u;
                    if (j == 0) { current_byte[c] = input_bank[c][bin_index[c] >> 3]; ctx[c] = 1; }
                    unsigned bit = ((unsigned)current_byte[c] >> (7 - j)) & 1u;
                    uint32_t lo = low[c], rg = range[c];
                    mc_ctx cs = tree[c][ctx[c]];
                    mc_steps tk = mc_code_bin(&lo, &rg, &cs, (int)bit);
                    low[c] = lo; range[c] = rg; tree[c][ctx[c]] = cs;
                    ctx[c] = (uint16_t)((ctx[c] << 1) | bit);
                    cls[c] = tk.cls; rem[c] = (uint8_t)tk.s;
                    bin_index[c]++;
                } else {                              // terminate(1) + first flush renorm
                    uint32_t lo = low[c], rg = range[c];
                    rg = (rg - 2u) & MC_RANGE_MASK;
                    lo = (lo + rg) & MC_LOW_MASK;
                    rg = 2u;
                    mc_steps tk = mc_renorm_classify(&lo, &rg);
                    low[c] = lo; range[c] = rg;
                    cls[c] = tk.cls; rem[c] = (uint8_t)tk.s;
                    stage[c] = S_FLUSH2;
                }
            } else if (stg == S_FLUSH2) {             // flush tail token
                mc_steps tk = mc_flush_tail(low[c]);
                cls[c] = tk.cls; rem[c] = (uint8_t)tk.s;
                stage[c] = S_PAD;
            } else {                                  // S_PAD: pad partial byte, finish
                if (nacc[c] != 0) {
                    output_bank[c][out_index[c]++] = (mc_byte)(acc[c] << (8 - nacc[c]));
                    nacc[c] = 0;
                }
                output_len[c] = out_index[c];
                stage[c] = S_DONE;
                done_count[g]++;
            }
        }
    }

    // --- header [LANES x {u16 rlen, u16 clen}] then chunks (matches mc_encode) ---
    int oi = 0;
Header:
    for (int c = 0; c < LANES; c++) {
        out[oi++] = (mc_byte)( input_len[c]        & 0xFF);
        out[oi++] = (mc_byte)((input_len[c]  >> 8) & 0xFF);
        out[oi++] = (mc_byte)( output_len[c]       & 0xFF);
        out[oi++] = (mc_byte)((output_len[c] >> 8) & 0xFF);
    }
Store:
    for (int c = 0; c < LANES; c++)
        for (int i = 0; i < output_len[c]; i++) out[oi++] = output_bank[c][i];

    return oi;
}
