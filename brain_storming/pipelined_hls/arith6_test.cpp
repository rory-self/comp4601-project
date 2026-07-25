/* V6 testbench: (1) assert the pipelined coder's output is BYTE-IDENTICAL to
 * the V5 reference encoder (so decoder/host/demo work unchanged), and
 * (2) verify lossless round-trip through the V5 decoder. */
#include <stdio.h>
#include <string.h>
#include "arith3.h"

#ifndef KWAY
#define KWAY 4
#endif
#define CHUNK_CAP  (MAX_IN / KWAY + 64)

int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);  // DUT (arith6.cpp)

/* ---------- reference encoder: V5 semantics, using encode_bit/put_bit ---------- */
static int ref_encode_chunk(const byte_t *in, int n, byte_t *out) {
    uint32_t flag_prob = PROB_INIT, tree[NTREE];
    for (int i = 0; i < NTREE; i++) tree[i] = PROB_INIT;
    uint32_t ob = 0; int nb = 0, oi = 0;
    uint32_t low = 0, high = TOP_VALUE, pending = 0;
    for (int k = 0; k < n; k++) {
        encode_bit(low, high, pending, ob, nb, out, oi, flag_prob, 1);
        int b = in[k], ctx = 1;
        for (int j = 7; j >= 0; j--) {
            int bit = (b >> j) & 1;
            encode_bit(low, high, pending, ob, nb, out, oi, tree[ctx], bit);
            ctx = (ctx << 1) | bit;
        }
    }
    encode_bit(low, high, pending, ob, nb, out, oi, flag_prob, 0);
    pending++;
    if (low < FIRST_QTR) { put_bit(0, ob, nb, out, oi); while (pending > 0) { put_bit(1, ob, nb, out, oi); pending--; } }
    else                 { put_bit(1, ob, nb, out, oi); while (pending > 0) { put_bit(0, ob, nb, out, oi); pending--; } }
    if (nb > 0) out[oi++] = (byte_t)(ob << (8 - nb));
    return oi;
}
static byte_t rbuf[KWAY][CHUNK_CAP], rcout[KWAY][CHUNK_CAP];
static int ref_encode(const byte_t *in, int n, byte_t *out) {
    int clen[KWAY];
    int chunk = (n + KWAY - 1) / KWAY;
    for (int c = 0; c < KWAY; c++) {
        int start = c * chunk;
        int len = n - start; if (len > chunk) len = chunk; if (len < 0) len = 0;
        for (int i = 0; i < len; i++) rbuf[c][i] = in[start + i];
        clen[c] = ref_encode_chunk(rbuf[c], len, rcout[c]);
    }
    int oi = 0;
    for (int c = 0; c < KWAY; c++) { out[oi++] = (byte_t)(clen[c] & 0xFF); out[oi++] = (byte_t)((clen[c] >> 8) & 0xFF); }
    for (int c = 0; c < KWAY; c++)
        for (int i = 0; i < clen[c]; i++) out[oi++] = rcout[c][i];
    return oi;
}

/* ---------- decoder (same as arith5_test.cpp) ---------- */
static long g_bit; static const byte_t *g_in; static int g_len;
static inline uint32_t gb() { int bi=(int)(g_bit>>3), off=7-(int)(g_bit&7); g_bit++;
    return (uint32_t)((bi<g_len)?((g_in[bi]>>off)&1):0); }
static inline int dbit(uint32_t &low, uint32_t &high, uint32_t &code, uint32_t &prob) {
    uint32_t range=high-low+1, split=(range*prob)>>PROB_BITS; int bit;
    if ((code-low)<split){bit=0;high=low+split-1;} else {bit=1;low=low+split;}
    for(;;){ if(high<HALF){} else if(low>=HALF){code-=HALF;low-=HALF;high-=HALF;}
        else if(low>=FIRST_QTR&&high<THIRD_QTR){code-=FIRST_QTR;low-=FIRST_QTR;high-=FIRST_QTR;} else break;
        low=(low<<1)&TOP_VALUE; high=((high<<1)|1)&TOP_VALUE; code=((code<<1)|gb())&TOP_VALUE; }
    if(bit==0) prob+=(PROB_TOTAL-prob)>>MOVE_BITS; else prob-=prob>>MOVE_BITS;
    return bit;
}
static int decode_chunk(const byte_t *in, int len, byte_t *out) {
    g_in=in; g_len=len; g_bit=0;
    uint32_t flag=PROB_INIT, tree[NTREE]; for(int i=0;i<NTREE;i++) tree[i]=PROB_INIT;
    uint32_t low=0,high=TOP_VALUE,code=0;
    for(int i=0;i<CODE_BITS;i++) code=(code<<1)|gb();
    int on=0;
    for(;;){ if(!dbit(low,high,code,flag)) break;
        int ctx=1,b=0; for(int j=7;j>=0;j--){int bit=dbit(low,high,code,tree[ctx]); b=(b<<1)|bit; ctx=(ctx<<1)|bit;}
        out[on++]=(byte_t)b; }
    return on;
}
static int decode_kway(const byte_t *comp, int comp_len, byte_t *out) {
    (void)comp_len;
    int clen[KWAY], off = 2*KWAY, on = 0;
    for (int c=0;c<KWAY;c++) clen[c] = comp[2*c] | (comp[2*c+1]<<8);
    for (int c=0;c<KWAY;c++){ on += decode_chunk(comp+off, clen[c], out+on); off += clen[c]; }
    return on;
}

/* ---------- test driver ---------- */
// Work-unit counters. Strong definitions live in arith6.cpp (host build);
// these weak fallbacks keep the tb linkable in RTL co-simulation, where the
// DUT source is not compiled as software (counters then just read 0).
__attribute__((weak)) long g_flat_units = 0;
__attribute__((weak)) long g_flat_units_max = 0;
static byte_t enc[MAX_OUT], ref[MAX_OUT], dec[MAX_IN];
static int run_case(const char *name, const byte_t *msg, int n) {
    g_flat_units = 0; g_flat_units_max = 0;
    int cl = arith_encode(msg, n, enc);
    int rl = ref_encode(msg, n, ref);
    int exact = (cl == rl) && (memcmp(enc, ref, cl) == 0);
    int dn = decode_kway(enc, cl, dec);
    int ok = (dn == n) && (memcmp(msg, dec, n) == 0);
    // At II=1 one work unit ~= one cycle; the slowest chunk sets the pace.
    printf("%-12s in=%4d comp=%4d ratio=%6.2f%% bit-exact-vs-V5=%s round-trip=%s"
           "  units/sym=%.1f (worst chunk %.1f)\n",
           name, n, cl, 100.0*cl/(n?n:1), exact?"OK":"FAIL", ok?"OK":"FAIL",
           (double)g_flat_units/(n?n:1), (double)g_flat_units_max*KWAY/(n?n:1));
    return exact && ok;
}
int main() {
    printf("KWAY=%d (V6 pipelined flat coder)\n", KWAY);
    int ok = 1;
    { byte_t m[2048]; for(int i=0;i<2048;i++) m[i]='A'+(i%3); ok&=run_case("repetitive",m,2048); }
    { byte_t m[2000]; const char *t="the quick brown fox jumps over the lazy dog. "; int L=(int)strlen(t);
      for(int i=0;i<2000;i++) m[i]=t[i%L]; ok&=run_case("text",m,2000); }
    { byte_t m[2048]; unsigned r=7; for(int i=0;i<2048;i++){r=r*1103515245u+12345u; m[i]=(r>>16)&0xFF;} ok&=run_case("random",m,2048); }
    { byte_t m[MAX_IN]; unsigned r=99; for(int i=0;i<MAX_IN;i++){r=r*1103515245u+12345u;
        m[i]=(byte_t)((i&512)?('a'+(i%7)):((r>>16)&0xFF)); } ok&=run_case("mixed-full",m,4095); }
    { byte_t m[10]; for(int i=0;i<10;i++) m[i]='x'; ok&=run_case("tiny",m,10); }
    { byte_t m[1]={0}; ok&=run_case("empty",m,0); }
    printf("----------------------------------------------\n");
    printf(ok?"PASS: bit-exact with V5 and lossless\n":"FAIL\n");
    return ok?0:1;
}
