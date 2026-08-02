/*
 * Phase 2 testbench: drives the kernel for csim and cosim.
 *
 * Self-checking (returns non-zero on any mismatch) because cosim needs that to
 * report PASS.  Verification is a real round-trip through ../mcoder_dec.cpp,
 * the same independent decoder Phase 1 used -- not a golden-vector compare, so
 * it stays honest if the kernel is edited.
 *
 * TB_N defaults to 1024 bytes so cyc/sym from cosim is directly comparable to
 * V5's 83.7 cyc/sym at K=1 in results/sweep_big_results.csv.
 */
#include <stdio.h>
#include <string.h>
#include "mcoder.h"

#ifndef TB_N
#define TB_N 1024
#endif

int mc_encode(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT]);
int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

/* No extern into the kernel here: under cosim the kernel is RTL and any such
 * symbol fails to link.  The bin count is 8*n by construction anyway. */

static mc_byte msg[MC_MAX_IN], enc[MC_MAX_OUT], dec[MC_MAX_IN];

static int run_case(const char *name, int n) {
    int cl = mc_encode(msg, n, enc);
    int dn = mc_decode(enc, cl, dec);
    int ok = (dn == n) && (memcmp(msg, dec, n) == 0);
    printf("%-12s in=%4d comp=%4d ratio=%6.2f%% round-trip=%s\n",
           name, n, cl, 100.0 * cl / (n ? n : 1), ok ? "OK" : "FAIL");
    return ok;
}

int main() {
    printf("V7 M-coder kernel   KWAY=%d  TB_N=%d\n", MC_KWAY, TB_N);
    int ok = 1;

    /* text: the case the demo image and real corpus behave like */
    {
        const char *t = "the quick brown fox jumps over the lazy dog. ";
        int L = (int)strlen(t);
        for (int i = 0; i < TB_N; i++) msg[i] = (mc_byte)t[i % L];
        ok &= run_case("text", TB_N);
    }
    /* Extra data profiles. */

    for (int i = 0; i < TB_N; i++) msg[i] = (mc_byte)('A' + (i % 3));
    ok &= run_case("repetitive", TB_N);

    { unsigned r = 7;
      for (int i = 0; i < TB_N; i++) { r = r * 1103515245u + 12345u; msg[i] = (mc_byte)((r >> 16) & 0xFF); } }
    ok &= run_case("random", TB_N);

    memset(msg, 0, TB_N);
    ok &= run_case("all-zero", TB_N);

    for (int i = 0; i < TB_N; i++) msg[i] = (mc_byte)((i & 1) ? 0xFF : 0x00);
    ok &= run_case("alternating", TB_N);

    for (int i = 0; i < 10; i++) msg[i] = 'x';
    ok &= run_case("tiny", 10);

    /* At II=1 the coder runs 8 bins = 8 cycles per byte per chunk, and the K
     * chunks run concurrently, so the ideal is 8/K cycles per input byte.
     * Cosim reports what the RTL actually does, including the packer and the
     * AXI copy loops. */
    printf("----------------------------------------------\n");
    printf("ideal at II=1: %d bins/chunk -> %.2f cyc/byte at K=%d\n",
           8 * TB_N / MC_KWAY, 8.0 / MC_KWAY, MC_KWAY);
    printf("V5 measured 83.70 cyc/byte at K=1 (results/sweep_big_results.csv)\n");
    printf("----------------------------------------------\n");
    printf("%s\n", ok ? "PASS: lossless" : "FAIL");
    return ok ? 0 : 1;
}
