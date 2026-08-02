#include <cstdio>
#include "tans_table.h"
typedef unsigned char byte_t;
int tans_encode(const byte_t in[4096], int n, byte_t out[8192]);
int main(){
  static byte_t in[4096], out[8192];
  for(int i=0;i<4095;i++) in[i]=(byte_t)((i*7+13)&0xff);   // all symbols present
  int len=tans_encode(in,4095,out);
  printf("cosim tANS: n=4095 -> %d bytes\n", len);
  return 0;
}
