/*
 * Phase 1 validation harness.
 *
 * Three jobs:
 *   1. Lossless round-trip: encode -> decode -> memcmp, M-coder and V5 both.
 *   2. Compression cost of the ROM quantisation, measured against V5 on the
 *      same corpus.
 *   3. Work counters and a cycle model, so we know whether the algorithm can
 *      hit ~9-12 cycles/byte before spending time on synthesis.
 *
 * Cycle model (stated explicitly so the numbers can be argued with):
 *
 *   V5   cycles = bins + renorm_iters + pending_bits
 *        One cycle to update the interval, then the renorm loop carries a
 *        data-dependent break so HLS schedules it as a sequential loop --
 *        one cycle per iteration executed -- and the while(pending) drains
 *        cost one cycle per bit emitted.
 *
 *   MC   cycles = bins + stalls
 *        The bin loop is a fixed-latency body (ROM read, subtract, barrel
 *        shift, 8-deep unrolled emit) with no data-dependent trip count, so it
 *        pipelines at II=1: one cycle per bin.  `stalls` counts the only thing
 *        that can break that -- a bin whose carry drain has to retire more
 *        than one whole output byte, which needs more than its one slot on the
 *        output port.
 *
 * The V5 side of the model is calibrated against hardware: sweep_big_results
 * measured 83.7 cycles/byte at K=1.  Whatever ratio the model misses by is
 * V5's un-pipelined per-step overhead, and it is reported rather than hidden.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mcoder.h"
#include "v5_prof.h"

#define V5_MEASURED_CYC_PER_BYTE 83.7   /* results/sweep_big_results.csv, K=1 */

static mc_byte enc_mc[MC_MAX_OUT], dec_mc[MC_MAX_IN];
static v5_byte enc_v5[V5_MAX_OUT], dec_v5[V5_MAX_IN];

typedef struct {
    long bytes;
    long mc_comp, v5_comp;
    long mc_bins, mc_cycles, mc_stalls, mc_renorm, mc_emit_bits, mc_max_run;
    long v5_bins, v5_cycles, v5_renorm, v5_pending, v5_mults;
    int  mc_ok, v5_ok, cases;
} totals_t;

static void run_case(const char *name, const mc_byte *msg, int n, totals_t *T, int verbose) {
    memset(&g_mc_prof, 0, sizeof(g_mc_prof));
    memset(&g_v5_prof, 0, sizeof(g_v5_prof));

    int mc_len = mc_encode(msg, n, enc_mc);
    int mc_n   = mc_decode(enc_mc, mc_len, dec_mc);
    int mc_ok  = (mc_n == n) && (memcmp(msg, dec_mc, n) == 0);

    int v5_len = v5_encode(msg, n, enc_v5);
    int v5_n   = v5_decode(enc_v5, v5_len, dec_v5);
    int v5_ok  = (v5_n == n) && (memcmp(msg, dec_v5, n) == 0);

    long mc_cyc = g_mc_prof.bins + g_mc_prof.stalls;
    long v5_cyc = g_v5_prof.bins + g_v5_prof.renorm_iters + g_v5_prof.pending_bits;

    if (verbose) {
        printf("  %-14s %5d  %5d %6.2f%%   %5d %6.2f%%  %+6.2f%%   %s %s\n",
               name, n,
               v5_len, 100.0 * v5_len / (n ? n : 1),
               mc_len, 100.0 * mc_len / (n ? n : 1),
               100.0 * (mc_len - v5_len) / (v5_len ? v5_len : 1),
               mc_ok ? "OK" : "FAIL", v5_ok ? "OK" : "FAIL");
    }

    T->cases++;
    T->bytes        += n;
    T->mc_comp      += mc_len;      T->v5_comp    += v5_len;
    T->mc_bins      += g_mc_prof.bins;
    T->mc_renorm    += g_mc_prof.renorm_steps;
    T->mc_emit_bits += g_mc_prof.emit_bits;
    T->mc_stalls    += g_mc_prof.stalls;
    if (g_mc_prof.max_run > T->mc_max_run) T->mc_max_run = g_mc_prof.max_run;
    T->mc_cycles    += mc_cyc;
    T->v5_bins      += g_v5_prof.bins;
    T->v5_renorm    += g_v5_prof.renorm_iters;
    T->v5_pending   += g_v5_prof.pending_bits;
    T->v5_mults     += g_v5_prof.mults;
    T->v5_cycles    += v5_cyc;
    T->mc_ok        += mc_ok;
    T->v5_ok        += v5_ok;
}

/* Split a file into MC_MAX_IN blocks and run each as a case. */
static int run_file(const char *path, totals_t *T, int verbose) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static mc_byte buf[MC_MAX_IN];
    int blk = 0;
    for (;;) {
        size_t got = fread(buf, 1, MC_MAX_IN, f);
        if (got == 0) break;
        char nm[64];
        snprintf(nm, sizeof(nm), "%.10s#%d", strrchr(path, '/') ? strrchr(path, '/') + 1 : path, blk);
        run_case(nm, buf, (int)got, T, verbose);
        blk++;
        if (got < MC_MAX_IN) break;
    }
    fclose(f);
    return blk;
}

static void report(const char *title, const totals_t *T) {
    if (T->bytes == 0) return;
    double n = (double)T->bytes;
    printf("\n%s  (%d cases, %ld bytes)\n", title, T->cases, T->bytes);
    printf("  round-trip           MC %d/%d   V5 %d/%d\n",
           T->mc_ok, T->cases, T->v5_ok, T->cases);
    printf("  compressed size      V5 %ld (%.2f%%)   MC %ld (%.2f%%)   delta %+.3f%%\n",
           T->v5_comp, 100.0 * T->v5_comp / n,
           T->mc_comp, 100.0 * T->mc_comp / n,
           100.0 * (T->mc_comp - T->v5_comp) / (double)T->v5_comp);
    printf("  bins / byte          V5 %.3f              MC %.3f\n",
           T->v5_bins / n, T->mc_bins / n);
    printf("  renorm steps / byte  V5 %.3f              MC %.3f\n",
           T->v5_renorm / n, T->mc_renorm / n);
    printf("  multiplies / byte    V5 %.3f              MC %.3f\n",
           T->v5_mults / n, 0.0);
    printf("  variable-len work    V5 %.3f pending-bits/byte   MC %.4f stalls/byte"
           " (longest carry run %ld)\n",
           T->v5_pending / n, T->mc_stalls / n, T->mc_max_run);
    printf("  modeled cycles/byte  V5 %.2f               MC %.2f      -> %.2fx\n",
           T->v5_cycles / n, T->mc_cycles / n,
           (double)T->v5_cycles / (double)(T->mc_cycles ? T->mc_cycles : 1));
    printf("  vs fabric baseline   V5 measured %.1f cyc/byte at K=1; the model\n",
           V5_MEASURED_CYC_PER_BYTE);
    printf("                       says %.2f, so ~%.1f cycles of V5's real cost sit\n",
           T->v5_cycles / n, V5_MEASURED_CYC_PER_BYTE - T->v5_cycles / n);
    printf("                       in schedule overhead the op counts don't see.\n");
    printf("                       If MC hits II=1, %.1f -> %.2f is %.1fx.  Only\n",
           V5_MEASURED_CYC_PER_BYTE, T->mc_cycles / n,
           V5_MEASURED_CYC_PER_BYTE / (T->mc_cycles / n));
    printf("                       Phase 2 synthesis can confirm the II=1.\n");
}

int main(int argc, char **argv) {
    /* mc_rlps4 is the packed form the hardware reads; mc_rangeTabLPS is the
     * readable source of truth it was generated from.  Prove they agree. */
    for (int s = 0; s < 64; s++)
        for (int q = 0; q < 4; q++)
            if ((int)MC_RLPS(mc_rlps4[s], (unsigned)q) != (int)mc_rangeTabLPS[s][q]) {
                printf("FAIL: mc_rlps4[%d] byte %d != mc_rangeTabLPS[%d][%d]\n", s, q, s, q);
                return 1;
            }

    printf("M-coder Phase 1 validation   KWAY=%d   bin structure: %s\n", MC_KWAY,
#ifdef MC_TERM_FLAG
           "9 bins/byte, continuation flag (identical to V5 -> isolates the engine)"
#else
           "8 bins/byte, length in header"
#endif
    );
    printf("=====================================================================\n");
    printf("  %-14s %5s  %5s %7s   %5s %7s  %7s   %s\n",
           "case", "in", "V5", "ratio", "MC", "ratio", "delta", "rt MC/V5");

    totals_t synth; memset(&synth, 0, sizeof(synth));
    {
        static mc_byte m[2048];
        for (int i = 0; i < 2048; i++) m[i] = (mc_byte)('A' + (i % 3));
        run_case("repetitive", m, 2048, &synth, 1);
    }
    {
        static mc_byte m[2000];
        const char *t = "the quick brown fox jumps over the lazy dog. ";
        int L = (int)strlen(t);
        for (int i = 0; i < 2000; i++) m[i] = (mc_byte)t[i % L];
        run_case("text", m, 2000, &synth, 1);
    }
    {
        static mc_byte m[2048];
        unsigned r = 7;
        for (int i = 0; i < 2048; i++) { r = r * 1103515245u + 12345u; m[i] = (mc_byte)((r >> 16) & 0xFF); }
        run_case("random", m, 2048, &synth, 1);
    }
    {
        static mc_byte m[4096];
        for (int i = 0; i < 4096; i++) m[i] = (mc_byte)0x00;
        run_case("all-zero", m, 4096, &synth, 1);
    }
    {
        /* Worst case for the carry path: long runs that straddle the midpoint. */
        static mc_byte m[4096];
        for (int i = 0; i < 4096; i++) m[i] = (mc_byte)((i & 1) ? 0xFF : 0x00);
        run_case("alternating", m, 4096, &synth, 1);
    }
    {
        static mc_byte m[10];
        for (int i = 0; i < 10; i++) m[i] = 'x';
        run_case("tiny", m, 10, &synth, 1);
    }

    /* Real data: files on the command line, else the demo image. */
    totals_t real; memset(&real, 0, sizeof(real));
    int nfiles = 0;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) nfiles += run_file(argv[i], &real, 0) ? 1 : 0;
    } else {
        /* Only stable inputs here.  Using this project's own sources as corpus
         * data made the numbers move every time the code was edited. */
        nfiles += run_file("../demo/image.pgm", &real, 0) ? 1 : 0;
    }

    report("SYNTHETIC CORPUS", &synth);
    if (real.cases) report("REAL CORPUS", &real);

    totals_t all = synth;
    all.cases += real.cases; all.bytes += real.bytes;
    all.mc_comp += real.mc_comp; all.v5_comp += real.v5_comp;
    all.mc_bins += real.mc_bins; all.mc_renorm += real.mc_renorm;
    all.mc_emit_bits += real.mc_emit_bits; all.mc_stalls += real.mc_stalls;
    all.mc_cycles += real.mc_cycles;
    if (real.mc_max_run > all.mc_max_run) all.mc_max_run = real.mc_max_run;
    all.v5_bins += real.v5_bins; all.v5_renorm += real.v5_renorm;
    all.v5_pending += real.v5_pending; all.v5_mults += real.v5_mults;
    all.v5_cycles += real.v5_cycles;
    all.mc_ok += real.mc_ok; all.v5_ok += real.v5_ok;
    report("COMBINED", &all);

    /* Machine-readable line for sweep.sh. */
    printf("\nCSV,%d,%d,%ld,%ld,%ld,%.2f,%.2f,%.3f\n",
           MC_KWAY,
#ifdef MC_TERM_FLAG
           1,
#else
           0,
#endif
           all.bytes, all.v5_comp, all.mc_comp,
           all.v5_cycles / (double)all.bytes,
           all.mc_cycles / (double)all.bytes,
           100.0 * (all.mc_comp - all.v5_comp) / (double)all.v5_comp);

    if (g_mc_prof.ctx_dist_violations)
        printf("FAIL: context reused within 8 bins %ld times (breaks the kernel's\n"
               "      HLS DEPENDENCE distance=8 assertion)\n", g_mc_prof.ctx_dist_violations);

    if (g_mc_prof.sm_violations)
        printf("FAIL: MPS-renorm shortcut violated %ld times\n", g_mc_prof.sm_violations);

    int fail = (all.mc_ok != all.cases);
    printf("\n%s\n", fail ? "FAIL: M-coder round-trip is not lossless"
                          : "PASS: every M-coder round-trip is lossless");
    return fail ? 1 : 0;
}
