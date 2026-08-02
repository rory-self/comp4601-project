/* Testbench for the board top-level.  Same round-trip check as the plain
 * kernel testbench, but through arith_kernel()'s 4-argument interface so the
 * out_len port is exercised too. */
#include <stdio.h>
#include <string.h>
#include "mcoder.h"

#ifndef TB_N
#define TB_N 1024
#endif

void arith_kernel(const mc_byte in[MC_MAX_IN], int n, mc_byte out[MC_MAX_OUT],
                  int out_len[1]);
int  mc_decode(const mc_byte *comp, int comp_len, mc_byte *out);

static mc_byte msg[MC_MAX_IN], enc[MC_MAX_OUT], dec[MC_MAX_IN];

static int run_case(const char *name, int n) {
    int len[1] = {0};
    arith_kernel(msg, n, enc, len);
    int dn = mc_decode(enc, len[0], dec);
    int ok = (dn == n) && (memcmp(msg, dec, n) == 0);
    printf("%-12s in=%4d comp=%4d ratio=%6.2f%% round-trip=%s\n",
           name, n, len[0], 100.0 * len[0] / (n ? n : 1), ok ? "OK" : "FAIL");
    return ok;
}

int main() {
    printf("V7 board top-level  KWAY=%d  TB_N=%d\n", MC_KWAY, TB_N);
    int ok = 1;
    {
        const char *t = "the quick brown fox jumps over the lazy dog. ";
        int L = (int)strlen(t);
        for (int i = 0; i < TB_N; i++) msg[i] = (mc_byte)t[i % L];
        ok &= run_case("text", TB_N);
    }
    { unsigned r = 7;
      for (int i = 0; i < TB_N; i++) { r = r * 1103515245u + 12345u; msg[i] = (mc_byte)((r >> 16) & 0xFF); } }
    ok &= run_case("random", TB_N);

    memset(msg, 0, TB_N);
    ok &= run_case("all-zero", TB_N);

    printf("%s\n", ok ? "PASS: lossless" : "FAIL");
    return ok ? 0 : 1;
}
