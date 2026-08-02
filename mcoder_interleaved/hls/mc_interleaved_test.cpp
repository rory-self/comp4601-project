#include <cstdio>
#include <cstring>
#include "arith_mc_interleaved.h"

#ifndef LANES
#define LANES 16
#endif

int mc_encode(const mc_byte*, int, mc_byte*);           // Bryan's replicated (MC_KWAY==LANES)
int mc_decode(const mc_byte*, int, mc_byte*);

static mc_byte in[MC_MAX_IN], comp_il[MC_MAX_OUT], comp_rep[MC_MAX_OUT], dec[MC_MAX_IN];

static int run(const char* name, int n) {
    int cil = arith_mc_encode_interleaved(in, n, comp_il);   // our interleaved
    int crp = mc_encode(in, n, comp_rep);                    // Bryan's replicated
    bool same = (cil == crp) && (memcmp(comp_il, comp_rep, cil) == 0);
    int dn = mc_decode(comp_il, cil, dec);                   // decode OUR output
    bool lossless = (dn == n) && (memcmp(in, dec, n) == 0);
    printf("%-8s n=%4d  interleaved=%4d  replicated=%4d  %s  decode=%s\n",
           name, n, cil, crp,
           same ? "IDENTICAL" : "DIFFER!!",
           lossless ? "LOSSLESS" : "FAIL");
    return same && lossless;
}

int main() {
    int ok = 1;
    for (int i = 0; i < 4095; i++) in[i] = 'a' + i % 7;
    ok &= run("pattern", 4095);
    unsigned r = 7;
    for (int i = 0; i < 2048; i++) { r = r * 1103515245u + 12345u; in[i] = (r >> 16) & 0xff; }
    ok &= run("random", 2048);
    for (int i = 0; i < 100; i++) in[i] = (i * 37) & 0xff;
    ok &= run("mixed", 100);
    for (int i = 0; i < 10; i++) in[i] = 'x';
    ok &= run("tiny", 10);
    ok &= run("empty", 0);
    puts(ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
