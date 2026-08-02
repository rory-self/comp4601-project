#include <cstdio>
#include "mcoder.h"
int hybrid_encode_hls(const mc_byte*,int,mc_byte*);
int main(){ static mc_byte in[4096],out[8192];
  unsigned r=7; for(int i=0;i<4095;i++){r=r*1103515245u+12345u; in[i]=(r>>16)&0xff;}
  printf("hybrid: %d\n", hybrid_encode_hls(in,4095,out)); return 0; }
