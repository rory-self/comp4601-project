#ifndef MCODER_H_
#define MCODER_H_
#include <stdint.h>
#include "mcoder_tables.h"

/*
 * M-coder (H.264 CABAC arithmetic coding engine), K-way multi-stream.
 *
 * Drop-in replacement for the V5 exact coder (best_hls/arith3.h + arith5.cpp).
 * Same algorithm class -- adaptive binary arithmetic coding over an 8-level
 * bit-tree -- but three things change, and all three are why it is faster:
 *
 *   1. Interval split: V5 computes  split = (range * prob) >> 12, a 17x12
 *      multiply on the critical path.  Here it is  rLPS = ROM[state][qIdx],
 *      one 256-byte table read.  No multiplier, no DSP, no divide.
 *
 *   2. Renormalisation: V5 runs a data-dependent  while (needs shift)  loop
 *      (bounded at 32 trips) that shifts by one bit per iteration.  Here the
 *      shift count comes from a leading-zero count on the 9-bit range, and the
 *      range update is a single barrel shift.  The bit-emit side is a
 *      fixed-depth 8-deep unrolled block, so the loop has no data-dependent
 *      trip count and can be flattened into the pipeline.
 *
 *   3. Model update: V5 does  prob += (4096 - prob) >> 5  /  prob -= prob >> 5.
 *      Here it is  state = ROM[state], two 64-byte tables.  Context state
 *      shrinks from a 12-bit probability to a 6-bit state index plus a 1-bit
 *      MPS value -- 7 bits per context instead of 12, so the 256-entry context
 *      array halves in size and partitions more cheaply.
 *
 * Cost: the ROM quantises probability to 64 states and range to 4 buckets, so
 * compression is slightly worse than the exact coder.  Measured loss is
 * reported by mcoder_test.
 *
 * Container format (little-endian), matching V5's spirit but self-terminating
 * on an explicit raw length instead of a per-symbol continuation flag:
 *
 *   [ K x { uint16 raw_len, uint16 comp_len } ][ chunk 0 ][ chunk 1 ] ...
 *
 * Dropping the continuation flag takes the bin count from 9 bins/byte (V5) to
 * 8 bins/byte, an 11% cut that is independent of the engine change.  Building
 * with -DMC_TERM_FLAG restores V5's per-symbol continuation flag (and drops the
 * raw length from the header), which makes the bin structure identical to V5
 * and so isolates what the ROM quantisation alone costs.  The test reports
 * both, so the two effects never get conflated.
 */

#define MC_MAX_IN    4096
#define MC_MAX_OUT   8192

#ifndef MC_KWAY
#define MC_KWAY      4
#endif

#define MC_HDR_BYTES  (4 * MC_KWAY)                 /* 2 x uint16 per chunk */
#define MC_CHUNK_CAP  (MC_MAX_IN / MC_KWAY + 64)    /* per-chunk output cap  */

#define MC_NTREE      256          /* bit-tree contexts, index 1..255 */

/* Engine constants.  range lives in [256, 510]; low in [0, 1024]. */
#define MC_RANGE_INIT 510u
#define MC_QUARTER    256u
#define MC_HALF       512u

typedef unsigned char mc_byte;

/*
 * Context state, packed:  (pStateIdx << 1) | valMPS.
 * 7 bits.  Init 0 == pStateIdx 0 (p_LPS = 0.5, i.e. equiprobable), valMPS 0.
 */
typedef unsigned char mc_ctx;
#define MC_CTX_INIT   ((mc_ctx)0)

/* ------------------------------------------------------------------ *
 * Optional work counters, for the cycle model in mcoder_test.
 * Compiled out entirely unless MC_PROFILE is defined, so the HLS build
 * never sees them.
 * ------------------------------------------------------------------ */
#ifdef MC_PROFILE
typedef struct {
    long bins;            /* binary decisions coded                         */
    long renorm_steps;    /* total renorm shift positions (sum of s)        */
    long emit_calls;      /* put_bit invocations                            */
    long emit_bits;       /* raw bits appended to the stream                */
    long bytes_out;       /* whole bytes retired through the output port    */
    long stalls;          /* cycles a bin had to wait on the output port    */
    long max_run;         /* longest deferred-carry run seen                */
    long mults;           /* variable-operand multiplies (should be zero)   */
    long sm_violations;   /* MPS-renorm shortcut broken (must stay 0)       */
} mc_prof_t;
extern mc_prof_t g_mc_prof;
#define MC_PROF(x) do { g_mc_prof.x; } while (0)
#else
#define MC_PROF(x) do { } while (0)
#endif

/* ------------------------------------------------------------------ *
 * Leading-zero count -> renorm shift amount.
 *
 * range is in [2, 510] on entry and must end up in [256, 511], so
 *   s = 8 - floor(log2(range)),  0 <= s <= 7.
 * Synthesises to an 8-way priority mux (a few LUT levels), not a loop.
 * ------------------------------------------------------------------ */
static inline int mc_shift_of(uint32_t range) {
    if (range & 0x100u) return 0;
    if (range & 0x080u) return 1;
    if (range & 0x040u) return 2;
    if (range & 0x020u) return 3;
    if (range & 0x010u) return 4;
    if (range & 0x008u) return 5;
    if (range & 0x004u) return 6;
    return 7;                       /* range == 2 or 3 */
}

/* ================================================================== *
 * Encoder
 * ================================================================== */

/*
 * The engine is split into two halves that share nothing but a token.
 *
 *   mc_renorm_classify()  owns low/range.  Fixed latency: an LZC, a barrel
 *                         shift, and a fixed 8-deep classification of the
 *                         shifted-out bit positions.  No output, no carry
 *                         state, no variable-trip loop -- so it pipelines.
 *
 *   mc_pack_steps()       owns the deferred-carry count and the byte packer.
 *                         Variable latency, because emitting a deferred run
 *                         of length m costs bytes on the output port.
 *
 * Why split it this way: draining a deferred run is unbounded work, and any
 * variable-trip loop inside the coding loop stops HLS pipelining it (measured:
 * 128 cycles/bin when the drain sat inline).  The two halves cannot simply be
 * put in separate pipeline stages either, because one bin can emit several
 * runs, and several stream writes per iteration force II > 1.
 *
 * The way out is that the packer never needs `low`.  What each renorm step
 * emits is fully determined by its *classification* -- E1 emits a 0 then
 * drains, E2 emits a 1 then drains, E3 defers one bit -- so the classification
 * of a bin's <=8 steps is a fixed-width token (3 bits of count, 2 bits per
 * step) and the packer can maintain `outstanding` on its own.  One token per
 * bin, one stream write, II=1.
 *
 * Both the software path (mc_encode_bin, immediate packing) and the HLS
 * dataflow kernel are built from these same two primitives, so the Phase 1
 * round-trip corpus validates exactly the decomposition the hardware uses.
 */
#define MC_E1  0u   /* low <  QUARTER  -> emit 0, then drain the deferred run */
#define MC_E2  1u   /* low >= HALF     -> emit 1, then drain the deferred run */
#define MC_E3  2u   /* straddles       -> defer one more bit                  */

typedef struct {
    uint32_t cls;   /* 2 bits per step, step i at bits [2i+1:2i] */
    int      s;     /* number of valid steps, 0..8               */
} mc_steps;

/*
 * RenormE, bin-stage half: fixed LZC + barrel shift + fixed 8-deep classify.
 * Mutates low and range; produces the token.  This is the whole hot path, and
 * the 8-deep chain on `low` is the critical path of the entire design.
 *
 * Written as bit manipulation rather than arithmetic, deliberately.  The
 * obvious form -- compare against QUARTER/HALF, subtract, shift -- reads
 * better but synthesises to a chain of eight 32-bit comparators, subtractors
 * and selects, because low/range are plain uint32_t loop-carried state and
 * Vitis cannot prove the 10-bit bound through the phi merges.  Measured at
 * 18.5 ns against a 5 ns budget.  (arith6.cpp hit the same width-inference
 * wall on its multiply and papered over it with mask hints.)
 *
 * So the bounds are made explicit instead.  low is 10 bits and range 9, and
 * one step reduces to: test bit 9, shift left, mask.  Both the classification
 * and the update are pure bit tests:
 *
 *   b9=1        (low >= HALF)     -> E2.  Subtracting HALF clears bit 9, which
 *                                   the shift drops anyway, so mask 0x3FF.
 *   b9=0, b8=1  (straddle)        -> E3.  Subtracting QUARTER clears bit 8, so
 *                                   the new bit 9 must be forced to 0: 0x1FF.
 *   b9=0, b8=0  (low <  QUARTER)  -> E1.  Result is already < 512, so 0x1FF is
 *                                   a no-op and the mask need not be selected.
 *
 * which collapses the update to  L = (L << 1) & (b9 ? 0x3FF : 0x1FF)  -- one
 * bit test, one shift, one 2-way mask per step.
 */
#define MC_LOW_MASK    0x3FFu           /* low   is 10 bits: [0, 1022] */
#define MC_RANGE_MASK  0x1FFu           /* range is  9 bits: [2, 510]  */

/*
 * Classify the s shifted-out bit positions of low.  s comes from the caller,
 * because the two callers derive it differently -- see mc_code_bin.
 *
 * Solved in closed form rather than iterated.  The step recurrence is
 *
 *     L' = (L << 1) & (b9(L) ? 0x3FF : 0x1FF)
 *
 * (E2 clears bit 9, which the shift drops anyway; E3 clears bit 8, so the new
 * bit 9 must be forced to 0; E1's result is already < 512).  Written as eight
 * sequential steps that is eight levels of shift-mask-select, and synthesis
 * measured the resulting path at 5.79 ns against a 5 ns budget.
 *
 * But the recurrence unrolls.  The mask only ever touches bit 9, so the low
 * nine bits are just the original bits shifted:
 *
 *     L_i[8:0] = (L_0 << i) & 0x1FF
 *
 * and bit 9 follows b9(L_i) = b9(L_{i-1}) & b8(L_{i-1}), where b8(L_{i-1}) is
 * itself the original bit L_0[9-i].  Expanding that gives a prefix-AND:
 *
 *     L_i[9] = L_0[9] & L_0[8] & ... & L_0[9-i]
 *
 * So every step's classification is a function of two *original* bits -- the
 * prefix-AND up to i, and the constant-position bit L_0[8-i] -- and all eight
 * fall out of one depth-3 AND tree plus fixed wiring, in parallel.  The only
 * remaining shift is a single barrel shift by s.
 */
static inline mc_steps mc_renorm_low(uint32_t *low, int s) {
    mc_steps r;
    r.s = s;
    r.cls = 0;
    MC_PROF(renorm_steps += s);

    uint32_t L = *low & MC_LOW_MASK;

    /* p = prefix-AND; p at index i is bit 9 of the value entering step i. */
    uint32_t p = (L >> 9) & 1u;
    uint32_t ps = p;                       /* p at index s, for the new low */
Renorm:
    for (int i = 0; i < 8; i++) {
#pragma HLS UNROLL
        uint32_t b9i = p;                          /* bit 9 entering step i */
        uint32_t b8i = (L >> (8 - i)) & 1u;        /* constant select       */
        uint32_t c   = b9i ? MC_E2 : (b8i ? MC_E3 : MC_E1);
        if (i < s) r.cls |= c << (2 * i);
        p = b9i & b8i;                             /* prefix-AND advances   */
        if (i + 1 == s) ps = p;
    }

    *low = ((L << s) & (MC_LOW_MASK >> 1)) | (ps << 9);
    return r;
}

/* General form: derive s from range, then classify.  Used by the flush, where
 * range is forced to 2 and the shift is a full 7 places. */
static inline mc_steps mc_renorm_classify(uint32_t *low, uint32_t *range) {
    int s = mc_shift_of(*range & MC_RANGE_MASK);
    *range = (*range << s) & MC_RANGE_MASK;
    return mc_renorm_low(low, s);
}

/*
 * The three bits the H.264 flush writes after the final renorm:
 *   PutBit((low >> 9) & 1), then WriteBits(((low >> 7) & 3) | 1, 2).
 * The low bit of that 2-bit literal is always 1.  The two literals need no
 * drain, but encoding them as E1/E2 is harmless because the PutBit ahead of
 * them has already emptied `outstanding`.  So the flush tail is just three
 * more ordinary steps and needs no extra token kind.
 */
static inline mc_steps mc_flush_tail(uint32_t low) {
    mc_steps r;
    r.s = 3;
    uint32_t b0 = (low >> 9) & 1u;
    uint32_t b1 = (low >> 8) & 1u;
    r.cls = ((b0 ? MC_E2 : MC_E1) << 0)
          | ((b1 ? MC_E2 : MC_E1) << 2)
          | ( MC_E2               << 4);   /* the literal 1 */
    return r;
}

/* ---------------- packer half: owns the carry state ---------------- */

typedef struct {
    uint32_t acc;          /* bit accumulator                             */
    int      nacc;
    uint32_t outstanding;  /* deferred-carry bit count (bitsOutstanding)   */
    int      first;        /* firstBitFlag: suppress the leading bit       */
    mc_byte *out;
    int      oi;
} mc_pack;

static inline void mc_pack_init(mc_pack *p, mc_byte *out) {
    p->acc = 0; p->nacc = 0; p->outstanding = 0; p->first = 1;
    p->out = out; p->oi = 0;
}

/* Append n <= 24 bits, MSB-first.  At most 3 whole bytes fall out. */
static inline void mc_pack_bits(mc_pack *p, uint32_t v, int n) {
    uint32_t mask = (1u << n) - 1u;
    p->acc = (p->acc << n) | (v & mask);
    p->nacc += n;
    MC_PROF(emit_bits += n);
Drain:
    for (int k = 0; k < 3; k++) {
#pragma HLS UNROLL
        if (p->nacc < 8) break;
        p->out[p->oi++] = (mc_byte)(p->acc >> (p->nacc - 8));
        p->nacc -= 8;
        MC_PROF(bytes_out++);
    }
}

/* PutBit(b): emit b, then drain `outstanding` copies of !b. */
static inline void mc_pack_put(mc_pack *p, int b) {
    MC_PROF(emit_calls++);
    if (p->first) p->first = 0;
    else          mc_pack_bits(p, (uint32_t)(b & 1), 1);

    uint32_t m = p->outstanding;
#ifdef MC_PROFILE
    if ((long)m > g_mc_prof.max_run) g_mc_prof.max_run = (long)m;
#endif
Carry:
    while (m > 0) {
        /* Measured over the Phase 1 corpus: 0.0022 executions per input byte,
         * longest run 17 bits.  Unbounded in theory, which is exactly why this
         * lives in the packer stage and not in the coding loop. */
#pragma HLS LOOP_TRIPCOUNT min=0 max=2 avg=0
        int k = (m > 24u) ? 24 : (int)m;
        uint32_t pat = b ? 0u : ((1u << k) - 1u);
        mc_pack_bits(p, pat, k);
        m -= (uint32_t)k;
    }
    p->outstanding = 0;
}

/*
 * Expand one classification token into output bits.
 *
 * Bounded by st.s, not by 8.  The packer does not need II=1, but it does need
 * a low *average* cost, and a fixed 8-trip loop costs 8 whether or not the
 * steps are live -- synthesis estimated the packer at 2.76M cycles against the
 * coder's 33k on that basis.  Tokens are only pushed when s > 0, and the
 * measured average is ~1.07 steps per token, so bounding by s makes the packer
 * roughly 0.6 cycles per bin and it stops being the bottleneck.
 */
static inline void mc_pack_steps(mc_pack *p, mc_steps st) {
Steps:
    for (int i = 0; i < st.s; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=8 avg=1
        uint32_t c = (st.cls >> (2 * i)) & 3u;
        if (c == MC_E3) p->outstanding++;
        else            mc_pack_put(p, (c == MC_E2) ? 1 : 0);
    }
}

/* Pad the final partial byte. */
static inline void mc_pack_tail(mc_pack *p) {
    if (p->nacc > 0) {
        p->out[p->oi++] = (mc_byte)(p->acc << (8 - p->nacc));
        p->nacc = 0;
        MC_PROF(bytes_out++);
    }
}

/* ---------------- software path: classify + pack immediately ---------------- */

typedef struct {
    uint32_t low;          /* 11 bits: [0, 1024]  */
    uint32_t range;        /*  9 bits: [256, 510] */
    mc_pack  pk;
} mc_enc;

static inline void mc_enc_init(mc_enc *e, mc_byte *out) {
    e->low = 0; e->range = MC_RANGE_INIT;
    mc_pack_init(&e->pk, out);
}

/*
 * EncodeDecision (H.264 9.3.4.2): the hot path, and the whole of what the
 * hardware coding loop does.  One ROM read for the interval split, one
 * subtract, one conditional add, one ROM read for the state update, then the
 * fixed-latency renorm classify.  No multiply, no variable-trip loop.
 *
 * Kept separate from the packer so the HLS bin stage can call exactly this and
 * push the returned token, rather than duplicating the arithmetic.
 */
static inline mc_steps mc_code_bin(uint32_t *low, uint32_t *range,
                                   mc_ctx *cs, int bin) {
    unsigned st  = (unsigned)(*cs >> 1);
    unsigned mps = (unsigned)(*cs & 1);
    /* ROM addressed by st alone, so the read is off the loop-carried range
     * path; q only selects a byte from the word it returns.  See the comment
     * on mc_rlps4 in mcoder_tables.h. */
    uint32_t rw  = mc_rlps4[st];
    unsigned q   = (*range >> 6) & 3u;
    uint32_t rlps = MC_RLPS(rw, q);

    /*
     * Both candidate ranges and both normalisations are computed in parallel
     * and selected at the end, rather than selecting the range first and
     * normalising after.  Selecting first puts subtract -> priority encoder ->
     * barrel shift in series on the loop-carried path; synthesis measured that
     * at 4.62 ns against a 5 ns budget.
     *
     * The MPS side needs neither the encoder nor the shifter, because its
     * renorm is provably 0 or 1 bits.  q partitions range into
     * [256,319] [320,383] [384,447] [448,511], and rLPS is largest at state 0,
     * so the smallest possible rM = range - rLPS per quantile is
     *     256-128=128,  320-176=144,  384-208=176,  448-240=208,
     * all >= 128.  A 9-bit value >= 128 needs at most one doubling to reach
     * 256, so sM is just "is bit 8 clear".
     *
     * The LPS side's range is rlps itself, which does not depend on the
     * subtract at all, so its normalisation runs concurrently with it.
     */
    /* low + rM == low + range - rlps.  Forming (low + range) up front lets the
     * adder run concurrently with the ROM mux instead of queueing behind the
     * subtract, which removes one adder level from the loop-carried path. */
    uint32_t lr = *low + *range;                        /* 11 bits */

    uint32_t rM = (*range - rlps) & MC_RANGE_MASK;      /* MPS range */
    int      sM = (rM & 0x100u) ? 0 : 1;
#ifdef MC_PROFILE
    /* The sM shortcut above is only valid while rM >= 128.  Guard it so a
     * future edit to the tables cannot silently corrupt the stream. */
    if (sM != mc_shift_of(rM)) g_mc_prof.sm_violations++;
#endif
    uint32_t nM = (rM << sM) & MC_RANGE_MASK;
    int      sL = mc_shift_of(rlps);                    /* LPS range = rlps */
    uint32_t nL = (rlps << sL) & MC_RANGE_MASK;

    int lps = ((unsigned)(bin & 1) != mps);
    if (lps) {
        *low = (lr - rlps) & MC_LOW_MASK;
        if (st == 0) mps ^= 1u;            /* MPS flips at the 50/50 state */
        st = (unsigned)mc_transIdxLPS[st];
    } else {
        st = (unsigned)mc_transIdxMPS[st];
    }
    *range = lps ? nL : nM;
    *cs = (mc_ctx)((st << 1) | mps);
    MC_PROF(bins++);
    return mc_renorm_low(low, lps ? sL : sM);
}

/* Software path: code the bin, then pack its token immediately. */
static inline void mc_encode_bin(mc_enc *e, mc_ctx *cs, int bin) {
    mc_steps sp = mc_code_bin(&e->low, &e->range, cs, bin);
#ifdef MC_PROFILE
    /* One bin gets one output-port slot per cycle.  Anything beyond that is a
     * genuine pipeline stall, which is what the cycle model needs to know. */
    long bytes_before = g_mc_prof.bytes_out;
    mc_pack_steps(&e->pk, sp);
    long produced = g_mc_prof.bytes_out - bytes_before;
    if (produced > 1) g_mc_prof.stalls += produced - 1;
#else
    mc_pack_steps(&e->pk, sp);
#endif
}

/* EncodeTerminate(1) + EncodeFlush (H.264 9.3.4.5/9.3.4.6).  Pins the final
 * low value inside the interval so every preceding bin decodes. */
static inline void mc_enc_flush(mc_enc *e) {
    e->range = (e->range - 2u) & MC_RANGE_MASK;
    e->low   = (e->low + e->range) & MC_LOW_MASK;
    e->range = 2u;
    mc_pack_steps(&e->pk, mc_renorm_classify(&e->low, &e->range));
    mc_pack_steps(&e->pk, mc_flush_tail(e->low));
    mc_pack_tail(&e->pk);
}

/* ================================================================== *
 * Decoder
 * ================================================================== */

typedef struct {
    uint32_t range;        /* 9 bits  */
    uint32_t offset;       /* 9 bits, always < range */
    const mc_byte *in;
    int      len;
    long     bitpos;
} mc_dec;

static inline uint32_t mc_read_bits(mc_dec *d, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++) {
        int bi  = (int)(d->bitpos >> 3);
        int off = 7 - (int)(d->bitpos & 7);
        uint32_t b = (bi < d->len) ? ((uint32_t)(d->in[bi] >> off) & 1u) : 0u;
        d->bitpos++;
        v = (v << 1) | b;
    }
    return v;
}

static inline void mc_dec_init(mc_dec *d, const mc_byte *in, int len) {
    d->in = in; d->len = len; d->bitpos = 0;
    d->range  = MC_RANGE_INIT;
    d->offset = mc_read_bits(d, 9);   /* pairs with the encoder's firstBitFlag */
}

/* DecodeDecision (H.264 9.3.3.2).  Renorm is a single barrel shift on both
 * range and offset -- the decoder has no carry to resolve, so it is strictly
 * simpler than the encoder side. */
static inline int mc_decode_bin(mc_dec *d, mc_ctx *cs) {
    unsigned st  = (unsigned)(*cs >> 1);
    unsigned mps = (unsigned)(*cs & 1);
    unsigned q   = (d->range >> 6) & 3u;
    uint32_t rlps = (uint32_t)mc_rangeTabLPS[st][q];
    int bin;

    d->range -= rlps;
    if (d->offset >= d->range) {
        bin = (int)(mps ^ 1u);
        d->offset -= d->range;
        d->range   = rlps;
        if (st == 0) mps ^= 1u;
        st = (unsigned)mc_transIdxLPS[st];
    } else {
        bin = (int)mps;
        st = (unsigned)mc_transIdxMPS[st];
    }
    *cs = (mc_ctx)((st << 1) | mps);

    int s = mc_shift_of(d->range);
    d->range <<= s;
    d->offset = (d->offset << s) | mc_read_bits(d, s);
    return bin;
}

/* ================================================================== *
 * Top level
 * ================================================================== */

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

#endif /* MCODER_H_ */
