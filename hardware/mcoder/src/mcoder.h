#ifndef MCODER_H_
#define MCODER_H_
#include <stdint.h>
#include "mcoder_tables.h"

/* Block size.  Overridable from the build so the block-size sweep can be run
 * against a rebuilt kernel -- MC_MAX_IN sizes the buf[][]/cout[][] staging
 * arrays, so it is compiled into the RTL and cannot be raised host-side.
 * The container header carries rlen/clen as uint16, which caps this at 64 KB. */
#ifndef MC_MAX_IN
#define MC_MAX_IN    4096
#endif
#ifndef MC_MAX_OUT
#define MC_MAX_OUT   (2 * MC_MAX_IN)
#endif

#ifndef MC_KWAY
#define MC_KWAY      4
#endif

#define MC_HDR_BYTES  (4 * MC_KWAY)                 /* 2 x uint16 per chunk */
/* Per-chunk output cap.  Worst-case (near-random) input expands a chunk by
 * roughly 1.7-2%, converging with chunk size rather than staying a fixed
 * number of bytes -- so a fixed "+64" slack, sized for the original 512-byte
 * chunk (K=8, MC_MAX_IN=4096), silently overflows cout[][] once the chunk
 * crosses ~4 KB (measured: chunk=4096 needs 4168 B, "+64" gives 4160).
 * There is no bounds check on the packer's writes, so this was a silent
 * buffer overflow into the next chunk's memory, not a caught error.
 * chunk/8 (12.5%) plus a small flush allowance covers the measured worst
 * case (~2%) with a >5x margin at every block size tested (512 B..32 KB). */
#define MC_CHUNK_CAP  (MC_MAX_IN / MC_KWAY + (MC_MAX_IN / MC_KWAY) / 8 + 64)

#define MC_NTREE      256          /* bit-tree contexts, index 1..255 */

/* Engine constants.  range lives in [256, 510]; low in [0, 1022].
 * The QUARTER/HALF thresholds the classifier used to compare against are now
 * implicit in the bit tests of mc_renorm_low, so they are no longer defined. */
#define MC_RANGE_INIT 510u

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
    long emit_bits;       /* raw bits appended to the stream                */
    long bytes_out;       /* whole bytes retired through the output port    */
    long stalls;          /* cycles a bin had to wait on the output port    */
    long max_run;         /* longest deferred-carry run seen                */
    long sm_violations;   /* MPS-renorm shortcut broken (must stay 0)       */
    long ctx_dist_violations; /* two uses of one context < 8 bins apart      */
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


#define MC_E1  0u   /* low <  1/4  -> emit 0, then drain the deferred run */
#define MC_E2  1u   /* low >= 1/2  -> emit 1, then drain the deferred run */
#define MC_E3  2u   /* straddles   -> defer one more bit                   */

typedef struct {
    uint32_t cls;   /* 2 bits per step, step i at bits [2i+1:2i] */
    int      s;     /* number of valid steps, 0..8               */
} mc_steps;
#define MC_LOW_MASK    0x3FFu           /* low   is 10 bits: [0, 1022] */
#define MC_RANGE_MASK  0x1FFu           /* range is  9 bits: [2, 510]  */


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
