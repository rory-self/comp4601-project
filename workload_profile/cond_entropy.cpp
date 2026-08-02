// The CORRECT classifier for "can this bin skip the model?": conditional entropy
// of each bit-tree level given its context (the prefix bits), i.e. what the
// adaptive model can actually exploit. H(level|ctx) ~= 1 bit  =>  the model buys
// nothing  =>  route that bin to BYPASS (no context, no update, batchable k/cycle).
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
using namespace std;
static vector<uint8_t> rd(const string&p){ifstream f(p,ios::binary);
  if(p.size()>4&&p.substr(p.size()-4)==".pgm"){string m;int w,h,v;f>>m>>w>>h>>v;f.get();vector<uint8_t>px((size_t)w*h);f.read((char*)px.data(),px.size());return px;}
  f.seekg(0,ios::end);size_t n=f.tellg();f.seekg(0);vector<uint8_t>b(n);f.read((char*)b.data(),n);return b;}
int main(int argc,char**argv){
  printf("%-16s %7s | conditional entropy H(bit|ctx) per bit-tree level (MSB->LSB)      | adaptive  bypass  bins/byte\n","file","bytes");
  printf("---------------- ------- | L0    L1    L2    L3    L4    L5    L6    L7        |   (H<0.95)      (H>=0.95)\n");
  for(int a=1;a<argc;a++){
    auto d=rd(argv[a]); size_t n=d.size(); if(!n) continue;
    // counts[level][ctx][bit]; ctx at level L is the L-bit prefix (< 2^L)
    static long cnt[8][256][2];
    for(int l=0;l<8;l++)for(int c=0;c<256;c++)cnt[l][c][0]=cnt[l][c][1]=0;
    for(auto v:d){ int ctx=0;
      for(int l=0;l<8;l++){ int bit=(v>>(7-l))&1; cnt[l][ctx][bit]++; ctx=(ctx<<1)|bit; } }
    double Hl[8]; int nad=0,nby=0; string s;
    for(int l=0;l<8;l++){
      double tot=0,acc=0;
      for(int c=0;c<(1<<l);c++){ double a0=cnt[l][c][0],a1=cnt[l][c][1],t=a0+a1; if(t<=0)continue;
        double h=0; if(a0>0)h-=a0/t*log2(a0/t); if(a1>0)h-=a1/t*log2(a1/t); acc+=t*h; tot+=t; }
      Hl[l]= tot>0?acc/tot:0; if(Hl[l]<0.95) nad++; else nby++;
      char b[16]; snprintf(b,16,"%5.3f ",Hl[l]); s+=b;
    }
    string nm=argv[a]; auto sl=nm.find_last_of('/'); if(sl!=string::npos)nm=nm.substr(sl+1);
    printf("%-16s %7zu | %s| %8d %8d\n",nm.c_str(),n,s.c_str(),nad,nby);
  }
  printf("\nA bin whose H(bit|ctx) ~ 1.0 is incompressible GIVEN its context: the adaptive\n"
         "model cannot beat 1 bit, so it can be coded in BYPASS -- no context read/update,\n"
         "no probability state, and several bypass bins can be packed per cycle.\n");
  return 0;
}
