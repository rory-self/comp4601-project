/*
 * Phase 2: HLS kernel.  V7.
 *
 * Same top-level shape as best_hls/arith5.cpp (V5) and pipelined_hls/arith6.cpp
 * (V6) -- burst input into K banks, run K concurrent coders, concatenate behind
 * a length header -- so this drops into the existing board flow unchanged.
 *
 * What is different from V6:
 *
 *   V6 needed a flat state machine (S_CODE / S_RENORM / S_DRAIN / S_FLUSH)
 *   because V5's renorm loop had a data-dependent trip count and could not be
 *   pipelined.  One work unit per cycle, ~13 units per byte, and the interval
 *   split still carried a multiply that Vitis kept widening to 32x32 -- the
 *   thing the mask hints in arith6.cpp were fighting, and the reason timing
 *   did not close.
 *
 *   Here the coding loop does one *bin* per cycle -- 8 per byte -- and there is
 *   no multiply anywhere, so nothing for the width optimiser to get wrong and
 *   no DSP on the critical path.
 *
 * The two-stage split is not optional.  A first attempt kept the whole engine
 * in one loop; the deferred-carry drain is a variable-trip loop, HLS refused to
 * pipeline the coding loop around it, and the result was 128 cycles/bin at 52%
 * LUT.  So the engine is split at the point where the work stops being fixed
 * latency:
 *
 *   mc_bin_stage   owns low/range/contexts.  Fixed latency per bin, one stream
 *                  write per bin, pipelines at II=1.
 *   mc_pack_stage  owns the deferred-carry count and the byte packer.  Variable
 *                  latency, but it only has to average ~0.55 steps per bin, so
 *                  the FIFO absorbs the bursts and it never gates the coder.
 *
 * Both stages are built from the primitives in ../mcoder.h that the Phase 1
 * corpus validated (29/29 lossless, K in {1..16}), so this is the same
 * arithmetic, re-timed -- not a reimplementation.
 */
#include "../mcoder.h"
#include "ap_int.h"
#include "hls_stream.h"

#define HLS_CHUNK_CAP  MC_CHUNK_CAP
#define TOK_DEPTH      64

/* Trip-count hints (reporting only): exactly 8 bins per input byte. */
#define BINS_AVG  (8 * (MC_MAX_IN / MC_KWAY))
#define BINS_MAX  (8 * HLS_CHUNK_CAP)

/* One token per renormalising bin: the classification of its <=8 steps. */
struct mc_tok {
    ap_uint<16> cls;
    ap_uint<4>  s;
    ap_uint<1>  eos;
};

#ifndef __SYNTHESIS__
/* csim-only work counters.  At II=1 one Bins iteration is one clock, so these
 * predict the hardware cycle count from a software run -- same trick V6 used
 * with g_flat_units, kept so the two are directly comparable. */
long g_bin_units = 0;       /* bin-stage cycles, worst chunk  */
long g_tok_units = 0;       /* tokens pushed, worst chunk     */
#endif

/* ---------------- stage 1: the coder ---------------- */
static void mc_bin_stage(const mc_byte *in, int n, hls::stream<mc_tok> &tok) {
    mc_ctx tree[MC_NTREE];
#ifdef MC_CTX_LUTRAM
    /*
     * Distributed RAM instead of a 256-entry register file.  Complete
     * partitioning costs a 256:1 read mux and a 256-way write decode --
     * measured at 6759 LUT per coder, ~90% of the design.  As LUTRAM the same
     * array is 800.
     *
     * The read-modify-write then takes two cycles, which would force II=2
     * unless HLS knows how far apart two accesses to the same address can be.
     * It is exactly 8 iterations, and that is provable rather than hopeful:
     * within one byte ctx walks strictly down the bit-tree, so its 8 values are
     * distinct; and across bytes, bin position j only ever addresses
     * [2^j, 2^(j+1)-1], and those ranges are disjoint.  So the same context is
     * revisited only at the same bit position of the next byte -- 8 bins later.
     */
#ifdef MC_CTX_BRAM
#pragma HLS BIND_STORAGE variable=tree type=RAM_2P impl=BRAM
#else
#pragma HLS BIND_STORAGE variable=tree type=RAM_2P impl=LUTRAM
#endif
#pragma HLS DEPENDENCE variable=tree type=inter direction=RAW distance=8 dependent=true
#else
#pragma HLS ARRAY_PARTITION variable=tree complete
#endif
Init:
    for (int i = 0; i < MC_NTREE; i++) {
#pragma HLS UNROLL
        tree[i] = MC_CTX_INIT;
    }

    uint32_t low = 0, range = MC_RANGE_INIT;
    mc_byte b = 0;
    int ctx = 1;
    const int nbins = n << 3;

Bins:
    for (int t = 0; t < nbins; t++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=0 max=BINS_MAX avg=BINS_AVG
        int j = t & 7;
        if (j == 0) { b = in[t >> 3]; ctx = 1; }   /* new byte: reset bit-tree */
        int bit = (b >> (7 - j)) & 1;

        mc_steps sp = mc_code_bin(&low, &range, &tree[ctx], bit);
        ctx = (ctx << 1) | bit;

        /* One conditional write per iteration -- this is what keeps II=1. */
        if (sp.s) {
            mc_tok tk; tk.cls = sp.cls; tk.s = sp.s; tk.eos = 0;
            tok.write(tk);
#ifndef __SYNTHESIS__
            g_tok_units++;
#endif
        }
#ifndef __SYNTHESIS__
        g_bin_units++;
#endif
    }

    /* EncodeTerminate(1) + EncodeFlush, as tokens. */
    range -= 2u; low += range; range = 2u;
    mc_steps sf = mc_renorm_classify(&low, &range);
    if (sf.s) { mc_tok tk; tk.cls = sf.cls; tk.s = sf.s; tk.eos = 0; tok.write(tk); }
    mc_steps st = mc_flush_tail(low);
    { mc_tok tk; tk.cls = st.cls; tk.s = st.s; tk.eos = 0; tok.write(tk); }
    { mc_tok tk; tk.cls = 0;      tk.s = 0;    tk.eos = 1; tok.write(tk); }
}

/* ---------------- stage 2: the packer ---------------- */
/*
 * Flat, single pipelined loop: exactly one unit of work per cycle, where a
 * unit is "consume one classification step" or "emit one deferred-carry bit".
 *
 * The obvious structure -- a loop over tokens containing a loop over steps
 * containing the carry drain -- is functionally identical and is what
 * mc_pack_steps() does for software.  But three nested variable-trip loops
 * cost ~10 cycles of loop control per step in hardware, and cosim measured the
 * whole kernel at 25 cycles/byte with the coder itself sitting idle at 1
 * cycle/bin.  Flattening removes the nesting entirely.
 *
 * A token read is folded into the same iteration as its first step, so the
 * steady-state cost is ~0.58 cycles per bin (0.55 steps + a rare carry bit) --
 * comfortably under the coder's 1 bin per cycle, so the packer never gates it.
 *
 * This also drops the bounded-run assumption: carry runs are emitted one bit
 * per cycle for as long as they last, so an arbitrarily long run is correct,
 * just slow -- and the FIFO absorbs it.
 */
static void mc_pack_stage(hls::stream<mc_tok> &tok, mc_byte *out, int *len) {
    /* Every register is sized to what it actually holds.  Declaring these as
     * int/uint32_t costs 0.46 ns of slack for nothing -- the accumulator never
     * holds more than 7 bits before it is flushed, and the step index never
     * exceeds 8. */
    ap_uint<8>  acc  = 0;
    ap_uint<4>  nacc = 0;
    ap_uint<24> outstanding = 0, run = 0;   /* bounded by total output bits */
    ap_uint<1>  runbit = 0, first = 1;
    /* The token's classification word is consumed two bits at a time from the
     * bottom, rather than indexed as cls >> (2*i).  Indexing puts a 16-bit
     * variable barrel shift on the loop-carried path (measured 5.50 ns); a
     * constant shift by 2 is free wiring. */
    ap_uint<16> cls = 0;
    ap_uint<4>  rem = 0;                    /* steps left in current token */
    int         oi = 0;
    mc_tok      tk;
    bool        done = false;

Pack:
    while (!done) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=BINS_MAX avg=BINS_AVG
        bool       emit = false;           /* append a bit this cycle? */
        ap_uint<1> ebit = 0;

        /*
         * Exactly one of three things per cycle.  The token read deliberately
         * does NOT share a cycle with the first step: folding them saves ~0.5
         * cycles per bin, but puts the FIFO read output straight into the
         * classification logic, and that path measured 6.17 ns against the
         * 6.0 ns target.  Keeping them separate costs ~6% packer throughput
         * and closes timing.
         */
        if (run != 0) {                    /* draining a deferred-carry run */
            emit = true; ebit = runbit;
            run--;
        } else if (rem == 0) {             /* fetch the next token */
            tk = tok.read();
            if (tk.eos) done = true;
            else { cls = tk.cls; rem = tk.s; }
        } else {                           /* one classification step */
            ap_uint<2> c = cls & 3;
            cls >>= 2;
            rem--;
            if (c == MC_E3) {
                outstanding++;             /* straddle: defer one bit */
            } else {
                ap_uint<1> b = (c == MC_E2) ? 1 : 0;
                if (first) first = 0;      /* firstBitFlag suppresses this one */
                else       { emit = true; ebit = b; }
                runbit = ~b;
                run    = outstanding;      /* then !b, `outstanding` times */
                outstanding = 0;
            }
        }

        if (emit) {
            acc = (acc << 1) | ebit;
            nacc++;
            if (nacc == 8) { out[oi++] = (mc_byte)acc; nacc = 0; }
        }
    }
    if (nacc != 0) out[oi++] = (mc_byte)(acc << (8 - nacc));  /* pad tail */
    *len = oi;
}

static void mc_encode_chunk_hls(const mc_byte *in, int n, mc_byte *out, int *len) {
#pragma HLS DATAFLOW
    hls::stream<mc_tok> tok;
#pragma HLS STREAM variable=tok depth=TOK_DEPTH
    mc_bin_stage(in, n, tok);
    mc_pack_stage(tok, out, len);
}

/*
 * Output layout: [ K x { uint16 raw_len, uint16 comp_len } ][ chunk 0 ] ...
 * Carrying raw_len is what lets the decoder stop without a per-symbol
 * continuation flag, i.e. 8 bins/byte instead of V5/V6's 9.
 */
int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]) {
#pragma HLS INTERFACE m_axi port=in  offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1

    /* Burst input into K separate banks so the K coders read concurrently. */
    static mc_byte buf[MC_KWAY][HLS_CHUNK_CAP];
#pragma HLS ARRAY_PARTITION variable=buf dim=1 complete
    static mc_byte cout[MC_KWAY][HLS_CHUNK_CAP];
#pragma HLS ARRAY_PARTITION variable=cout dim=1 complete
    int rlen[MC_KWAY], clen[MC_KWAY];
#pragma HLS ARRAY_PARTITION variable=rlen dim=1 complete
#pragma HLS ARRAY_PARTITION variable=clen dim=1 complete

    int chunk = (n + MC_KWAY - 1) / MC_KWAY;
Split:
    for (int c = 0; c < MC_KWAY; c++) {
        int start = c * chunk;
        int len = n - start; if (len > chunk) len = chunk; if (len < 0) len = 0;
        for (int i = 0; i < len; i++) buf[c][i] = in[start + i];
        rlen[c] = len;
    }

    /* K concurrent coder instances. */
Encode_K:
    for (int c = 0; c < MC_KWAY; c++) {
#pragma HLS UNROLL
        mc_encode_chunk_hls(buf[c], rlen[c], cout[c], &clen[c]);
    }

    int oi = 0;
Header:
    for (int c = 0; c < MC_KWAY; c++) {
        out[oi++] = (mc_byte)( rlen[c]       & 0xFF);
        out[oi++] = (mc_byte)((rlen[c] >> 8) & 0xFF);
        out[oi++] = (mc_byte)( clen[c]       & 0xFF);
        out[oi++] = (mc_byte)((clen[c] >> 8) & 0xFF);
    }
Concat:
    for (int c = 0; c < MC_KWAY; c++)
        for (int i = 0; i < clen[c]; i++) out[oi++] = cout[c][i];

    return oi;
}
