#include <stdio.h>
#include <string.h>
#include "arith3.h"
#ifndef KWAY
#define KWAY 8
#endif
void arith_kernel(const byte_t*, int, byte_t*, int*);
static long g_bit; static const byte_t* g_in; static int g_len;
static inline uint32_t gb(){int bi=(int)(g_bit>>3),off=7-(int)(g_bit&7);g_bit++;return (uint32_t)((bi<g_len)?((g_in[bi]>>off)&1):0);}
static inline int dbit(uint32_t&lo,uint32_t&hi,uint32_t&co,uint32_t&pr){uint32_t r=hi-lo+1,sp=(r*pr)>>PROB_BITS;int b;
 if((co-lo)<sp){b=0;hi=lo+sp-1;}else{b=1;lo=lo+sp;}
 for(;;){if(hi<HALF){}else if(lo>=HALF){co-=HALF;lo-=HALF;hi-=HALF;}else if(lo>=FIRST_QTR&&hi<THIRD_QTR){co-=FIRST_QTR;lo-=FIRST_QTR;hi-=FIRST_QTR;}else break;
  lo=(lo<<1)&TOP_VALUE;hi=((hi<<1)|1)&TOP_VALUE;co=((co<<1)|gb())&TOP_VALUE;}
 if(b==0)pr+=(PROB_TOTAL-pr)>>MOVE_BITS;else pr-=pr>>MOVE_BITS;return b;}
static int dec_chunk(const byte_t*in,int len,byte_t*out){g_in=in;g_len=len;g_bit=0;
 uint32_t fl=PROB_INIT,tr[NTREE];for(int i=0;i<NTREE;i++)tr[i]=PROB_INIT;
 uint32_t lo=0,hi=TOP_VALUE,co=0;for(int i=0;i<CODE_BITS;i++)co=(co<<1)|gb();int on=0;
 for(;;){if(!dbit(lo,hi,co,fl))break;int ctx=1,b=0;for(int j=7;j>=0;j--){int bit=dbit(lo,hi,co,tr[ctx]);b=(b<<1)|bit;ctx=(ctx<<1)|bit;}out[on++]=(byte_t)b;}return on;}
static int dec_kway(const byte_t*c,int cl,byte_t*out){int L[KWAY],off=2*KWAY,on=0;
 for(int k=0;k<KWAY;k++)L[k]=c[2*k]|(c[2*k+1]<<8);
 for(int k=0;k<KWAY;k++){on+=dec_chunk(c+off,L[k],out+on);off+=L[k];}return on;}
static byte_t enc[MAX_OUT],dec[MAX_IN];
int main(){
 int ok=1;
 struct{const char*nm;int n;}cs[]={{"rep",2048},{"txt",2000},{"tiny",10}};
 for(int t=0;t<3;t++){int n=cs[t].n;byte_t m[4096];
   if(t==0)for(int i=0;i<n;i++)m[i]='A'+(i%3);
   else if(t==1){const char*s="the quick brown fox jumps over the lazy dog. ";int L=strlen(s);for(int i=0;i<n;i++)m[i]=s[i%L];}
   else for(int i=0;i<n;i++)m[i]='x';
   int cl=0;arith_kernel(m,n,enc,&cl);int dn=dec_kway(enc,cl,dec);
   int good=(dn==n)&&(memcmp(m,dec,n)==0);ok&=good;
   printf("%-4s in=%4d comp=%4d ratio=%6.2f%% round-trip=%s\n",cs[t].nm,n,cl,100.0*cl/n,good?"OK":"FAIL");}
 printf(ok?"PASS\n":"FAIL\n");return ok?0:1;}
