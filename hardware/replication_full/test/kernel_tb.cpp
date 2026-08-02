/* V5 testbench: decode the K-way multi-stream output, verify lossless round-trip. */
#include <stdio.h>
#include <string.h>
#include "arith3.h"

#ifndef KWAY
#define KWAY 4
#endif

int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);

// bit reader bound to one chunk
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
    int clen[KWAY], off = 2*KWAY, on = 0;
    for (int c=0;c<KWAY;c++) clen[c] = comp[2*c] | (comp[2*c+1]<<8);
    for (int c=0;c<KWAY;c++){ on += decode_chunk(comp+off, clen[c], out+on); off += clen[c]; }
    return on;
}

static byte_t enc[MAX_OUT], dec[MAX_IN];
static int run_case(const char *name, const byte_t *msg, int n){
    int cl = arith_encode(msg, n, enc);
    int dn = decode_kway(enc, cl, dec);
    int ok = (dn==n) && (memcmp(msg,dec,n)==0);
    printf("%-12s in=%4d comp=%4d ratio=%6.2f%% round-trip=%s\n", name, n, cl,
           100.0*cl/(n?n:1), ok?"OK":"FAIL");
    return ok;
}
int main(){
    printf("KWAY=%d\n", KWAY);
    int ok=1;
    { byte_t m[2048]; for(int i=0;i<2048;i++) m[i]='A'+(i%3); ok&=run_case("repetitive",m,2048); }
    { byte_t m[2000]; const char *t="the quick brown fox jumps over the lazy dog. "; int L=strlen(t);
      for(int i=0;i<2000;i++) m[i]=t[i%L]; ok&=run_case("text",m,2000); }
    { byte_t m[2048]; unsigned r=7; for(int i=0;i<2048;i++){r=r*1103515245u+12345u; m[i]=(r>>16)&0xFF;} ok&=run_case("random",m,2048); }
    { byte_t m[10]; for(int i=0;i<10;i++) m[i]='x'; ok&=run_case("tiny",m,10); }
    printf("----------------------------------------------\n");
    printf(ok?"PASS: all round-trips lossless\n":"FAIL\n");
    return ok?0:1;
}
