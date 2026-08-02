/* K-way replicated static tANS, board top: arith_kernel(in, n, out, out_len).
 * Each lane gets its OWN copy of the table (STATE/DNB/DFS) so the K coders read
 * in parallel -- a single shared ROM serialises them (1 read/cycle). Replication
 * is the right fit for tANS (cheap datapath): copy the tables, run K in parallel.
 * Container: [K x {u16 rlen, u16 clen}][chunk...]; chunk = [u16 total_bits][payload]. */
#include "tans_table.h"
#ifndef MAX_IN
#define MAX_IN   16384
#endif
#define MAX_OUT  (MAX_IN*2)
#ifndef KWAY
#define KWAY 4
#endif
#define CHUNK_CAP (MAX_IN / KWAY * 2 + 64)   /* worst-case tANS out ~1.5x in */
typedef unsigned char byte_t;

static int tans_one(const byte_t *buf, int n, byte_t *cout,
                    const uint16_t *stab, const uint32_t *dnb, const int32_t *dfs) {
#ifdef __SYNTHESIS__
#pragma HLS INLINE          /* so the unrolled Encode_K gives each lane its own datapath */
#endif
    if (n == 0) { cout[0]=0; cout[1]=0; return 2; }
    uint32_t s0=buf[n-1], nb0=(dnb[s0]+(1u<<15))>>16;
    uint32_t state=(nb0<<16)-dnb[s0]; state=stab[(state>>nb0)+dfs[s0]];
    uint32_t acc=0; int nacc=0, oi=2; uint32_t total_bits=0;
Enc:
    for (int i=n-2;i>=0;i--){
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
        uint32_t s=buf[i], nb=(state+dnb[s])>>16;
        acc=(acc<<nb)|(state&((1u<<nb)-1u)); nacc+=nb; total_bits+=nb;
        if(nacc>=8){cout[oi++]=(byte_t)(acc>>(nacc-8));nacc-=8;}
        if(nacc>=8){cout[oi++]=(byte_t)(acc>>(nacc-8));nacc-=8;}
        state=stab[(state>>nb)+dfs[s]];
    }
    acc=(acc<<TANS_L)|(state&(TANS_SIZE-1)); nacc+=TANS_L; total_bits+=TANS_L;
    if(nacc>=8){cout[oi++]=(byte_t)(acc>>(nacc-8));nacc-=8;}
    if(nacc>=8){cout[oi++]=(byte_t)(acc>>(nacc-8));nacc-=8;}
    if(nacc>0){cout[oi++]=(byte_t)(acc<<(8-nacc));}
    cout[0]=(byte_t)(total_bits&0xff); cout[1]=(byte_t)((total_bits>>8)&0xff);
    return oi;
}

void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]) {
#ifdef __SYNTHESIS__
#pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=out_len offset=slave bundle=gmem2
#endif
    // per-lane table copies so the K coders read in parallel
    static uint16_t st[KWAY][TANS_SIZE];
    static uint32_t dn[KWAY][256];
    static int32_t  df[KWAY][256];
    static byte_t buf[KWAY][CHUNK_CAP], cout[KWAY][CHUNK_CAP];
#ifdef __SYNTHESIS__
#pragma HLS ARRAY_PARTITION variable=st  dim=1 complete
#pragma HLS ARRAY_PARTITION variable=dn  dim=1 complete
#pragma HLS ARRAY_PARTITION variable=df  dim=1 complete
#pragma HLS ARRAY_PARTITION variable=buf dim=1 complete
#pragma HLS ARRAY_PARTITION variable=cout dim=1 complete
#endif
InitTab:
    for(int i=0;i<TANS_SIZE;i++){ for(int c=0;c<KWAY;c++){
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
        st[c][i]=TANS_STATE[i]; if(i<256){ dn[c][i]=TANS_DNB[i]; df[c][i]=TANS_DFS[i]; } } }

    int rlen[KWAY], clen[KWAY];
    int chunk=(n+KWAY-1)/KWAY;
Split:
    for(int c=0;c<KWAY;c++){ int start=c*chunk,len=n-start; if(len>chunk)len=chunk; if(len<0)len=0;
        for(int i=0;i<len;i++) buf[c][i]=in[start+i]; rlen[c]=len; }
Encode_K:
    for(int c=0;c<KWAY;c++){
#ifdef __SYNTHESIS__
#pragma HLS UNROLL
#endif
        clen[c]=tans_one(buf[c],rlen[c],cout[c],st[c],dn[c],df[c]); }
    int oi=0;
    for(int c=0;c<KWAY;c++){ out[oi++]=rlen[c]&0xff; out[oi++]=(rlen[c]>>8)&0xff; out[oi++]=clen[c]&0xff; out[oi++]=(clen[c]>>8)&0xff; }
    for(int c=0;c<KWAY;c++) for(int i=0;i<clen[c];i++) out[oi++]=cout[c][i];
    out_len[0]=oi;
}
