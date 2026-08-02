/*
 * SIMD (lockstep) K-way static tANS -- board top: arith_kernel(in,n,out,out_len).
 *
 * WHY THIS SHAPE. Two earlier attempts failed to parallelise, and the reasons are
 * the interesting part:
 *   1. K calls to an encode function inside `#pragma HLS UNROLL` did NOT run in
 *      parallel: HLS schedules the K independent *loops* sequentially (cosim
 *      199,340 cyc for K=8 == 8 x one lane). Unrolling a loop that contains a
 *      pipelined loop does not replicate the pipeline.
 *   2. Even with the function inlined, a single shared table ROM serialises the
 *      lanes: one read port, so K lanes cannot look up in the same cycle.
 *
 * FIX: one flat loop where each iteration advances ALL K lanes by one byte
 * (true SIMD), with every per-lane array completely partitioned and the table
 * REPLICATED per lane so K lookups happen concurrently. Now throughput is
 * K bytes per II, not K x (bytes per II).
 */
#include "tans_table.h"
#ifndef MAX_IN
#define MAX_IN   16384
#endif
#define MAX_OUT  (MAX_IN * 2)
#ifndef KWAY
#define KWAY 4
#endif
#define CHUNK_CAP (MAX_IN / KWAY * 2 + 64)    /* worst case tANS out ~1.5x in */
typedef unsigned char byte_t;

void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in      offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out     offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
#endif
    /* Per-lane table copies: K concurrent lookups need K read ports. */
    static uint16_t st[KWAY][TANS_SIZE];
    static uint32_t dn[KWAY][256];
    static int32_t  df[KWAY][256];
    static byte_t   buf[KWAY][CHUNK_CAP];
    static byte_t   cbuf[KWAY][CHUNK_CAP];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=st   dim=1 complete
#pragma HLS ARRAY_PARTITION variable=dn   dim=1 complete
#pragma HLS ARRAY_PARTITION variable=df   dim=1 complete
#pragma HLS ARRAY_PARTITION variable=buf  dim=1 complete
#pragma HLS ARRAY_PARTITION variable=cbuf dim=1 complete
#endif

InitTab:
    for (int i = 0; i < TANS_SIZE; i++) {
        for (int c = 0; c < KWAY; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            st[c][i] = TANS_STATE[i];
            if (i < 256) { dn[c][i] = TANS_DNB[i]; df[c][i] = TANS_DFS[i]; }
        }
    }

    int rlen[KWAY], oi[KWAY], nacc[KWAY];
    uint32_t state[KWAY], acc[KWAY], tbits[KWAY];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=rlen  complete
#pragma HLS ARRAY_PARTITION variable=oi    complete
#pragma HLS ARRAY_PARTITION variable=nacc  complete
#pragma HLS ARRAY_PARTITION variable=state complete
#pragma HLS ARRAY_PARTITION variable=acc   complete
#pragma HLS ARRAY_PARTITION variable=tbits complete
#endif

    const int chunk = (n + KWAY - 1) / KWAY;
Split:
    for (int c = 0; c < KWAY; c++) {
        int start = c * chunk, len = n - start;
        if (len > chunk) len = chunk;
        if (len < 0) len = 0;
        for (int i = 0; i < len; i++) buf[c][i] = in[start + i];
        rlen[c] = len;
    }

Init:
    for (int c = 0; c < KWAY; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
        oi[c] = 2; nacc[c] = 0; acc[c] = 0; tbits[c] = 0; state[c] = 0;
        if (rlen[c] > 0) {
            uint32_t s0 = buf[c][rlen[c] - 1];
            uint32_t nb0 = (dn[c][s0] + (1u << 15)) >> 16;
            uint32_t sN = (nb0 << 16) - dn[c][s0];
            state[c] = st[c][(sN >> nb0) + df[c][s0]];
        }
    }

    /* SIMD core: one iteration = one byte for EVERY lane. */
SimdEnc:
    for (int i = chunk - 2; i >= 0; i--) {
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
        for (int c = 0; c < KWAY; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
            if (i <= rlen[c] - 2) {
                uint32_t s  = buf[c][i];
                uint32_t nb = (state[c] + dn[c][s]) >> 16;
                uint32_t a  = (acc[c] << nb) | (state[c] & ((1u << nb) - 1u));
                int      na = nacc[c] + (int)nb;
                tbits[c] += nb;
                if (na >= 8) { cbuf[c][oi[c]++] = (byte_t)(a >> (na - 8)); na -= 8; }
                if (na >= 8) { cbuf[c][oi[c]++] = (byte_t)(a >> (na - 8)); na -= 8; }
                acc[c] = a; nacc[c] = na;
                state[c] = st[c][(state[c] >> nb) + df[c][s]];
            }
        }
    }

Flush:
    for (int c = 0; c < KWAY; c++) {
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
        if (rlen[c] > 0) {
            uint32_t a = (acc[c] << TANS_L) | (state[c] & (TANS_SIZE - 1));
            int na = nacc[c] + TANS_L; tbits[c] += TANS_L;
            if (na >= 8) { cbuf[c][oi[c]++] = (byte_t)(a >> (na - 8)); na -= 8; }
            if (na >= 8) { cbuf[c][oi[c]++] = (byte_t)(a >> (na - 8)); na -= 8; }
            if (na >  0) { cbuf[c][oi[c]++] = (byte_t)(a << (8 - na)); }
        }
        cbuf[c][0] = (byte_t)(tbits[c] & 0xff);
        cbuf[c][1] = (byte_t)((tbits[c] >> 8) & 0xff);
    }

    int o = 0;
Header:
    for (int c = 0; c < KWAY; c++) {
        out[o++] = (byte_t)(rlen[c] & 0xff);  out[o++] = (byte_t)((rlen[c] >> 8) & 0xff);
        out[o++] = (byte_t)(oi[c]   & 0xff);  out[o++] = (byte_t)((oi[c]   >> 8) & 0xff);
    }
Concat:
    for (int c = 0; c < KWAY; c++)
        for (int i = 0; i < oi[c]; i++) out[o++] = cbuf[c][i];
    out_len[0] = o;
}
