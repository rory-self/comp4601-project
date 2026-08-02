#include <cstdio>
#include <cstdint>
#include <cmath>
#define MAX_IN 16384
typedef unsigned char byte_t;
void arith_kernel(const uint64_t*, int, uint64_t*, int*);
int main(){ static uint64_t in[MAX_IN/8], out[MAX_IN*2/8]; int len[1]={0};
  byte_t* ib=(byte_t*)in;
  double pr[256];double sm=0;for(int s=0;s<256;s++){pr[s]=pow(0.95,fabs(s-128.0))+1e-4;sm+=pr[s];}
  double cdf[256];double c=0;for(int s=0;s<256;s++){c+=pr[s]/sm;cdf[s]=c;}
  unsigned r=999;for(int i=0;i<MAX_IN;i++){r=r*1103515245u+12345u;double u=(r>>8)/16777216.0;int s=0;while(s<255&&u>cdf[s])s++;ib[i]=s;}
  arith_kernel(in,MAX_IN,out,len); printf("wide: %d -> %d\n",MAX_IN,len[0]); return 0; }
