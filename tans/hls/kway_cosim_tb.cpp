#include <cstdio>
#include <cmath>
#include "tans_table.h"
#define MAX_IN 16384
#define MAX_OUT (MAX_IN*2)
typedef unsigned char byte_t;
void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]);
int main(){ static byte_t in[MAX_IN],out[MAX_OUT]; int len[1]={0};
  double pr[256];double sm=0;for(int s=0;s<256;s++){pr[s]=pow(0.95,fabs(s-128.0))+1e-4;sm+=pr[s];}
  double cdf[256];double c=0;for(int s=0;s<256;s++){c+=pr[s]/sm;cdf[s]=c;}
  unsigned rng=999;for(int i=0;i<MAX_IN;i++){rng=rng*1103515245u+12345u;double u=(rng>>8)/16777216.0;int s=0;while(s<255&&u>cdf[s])s++;in[i]=s;}
  arith_kernel(in,MAX_IN,out,len); printf("kway tANS: %d -> %d bytes\n",MAX_IN,len[0]); return 0; }
