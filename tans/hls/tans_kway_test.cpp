#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "tans.h"
#include "tans_table.h"
#ifndef MAX_IN
#define MAX_IN 16384
#endif
#ifndef MAX_OUT
#define MAX_OUT (MAX_IN*2)
#endif
#ifndef KWAY
#define KWAY 8
#endif
typedef unsigned char byte_t;
void arith_kernel(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int out_len[1]);
static std::vector<uint8_t> read_pgm(const std::string&p){std::ifstream f(p,std::ios::binary);std::string m;int w,h,v;f>>m>>w>>h>>v;f.get();std::vector<uint8_t>px((size_t)w*h);f.read((char*)px.data(),px.size());return px;}
int main(int argc,char**argv){
  // rebuild the SAME smoothed norm the table used
  auto px=read_pgm(argc>1?argv[1]:"../../replication_full/demo/image.pgm");
  long cnt[256]; for(int s=0;s<256;s++)cnt[s]=1; for(auto b:px)cnt[b]++; long tot=0; for(int s=0;s<256;s++)tot+=cnt[s];
  int norm[256]={0}; tans::normalize(cnt,tot,TANS_L,norm);
  tans::Dec dec; tans::build_dec(dec,norm,TANS_L);
  int ok=1,blocks=0,ok_dbg=0; long ti=0,to=0;
  for(size_t off=0;off<px.size();off+=MAX_IN){
    int N=(int)std::min((size_t)MAX_IN,px.size()-off);
    std::vector<byte_t> in(px.begin()+off,px.begin()+off+N),out(MAX_OUT); int len[1]={0};
    arith_kernel(in.data(),N,out.data(),len); ti+=N; to+=len[0];
    std::vector<byte_t> rec; int hoff=4*KWAY; int chunkbase=0;
    for(int c=0;c<KWAY;c++){ int rlen=out[4*c]|(out[4*c+1]<<8),clen=out[4*c+2]|(out[4*c+3]<<8);
      const byte_t* ch=&out[hoff]; hoff+=clen; long tb=ch[0]|(ch[1]<<8);
      auto bit=[&](long j){return (ch[2+(j>>3)]>>(7-(j&7)))&1;};
      long cur=tb; auto rd=[&](int nb){cur-=nb;uint32_t v=0;for(int k=0;k<nb;k++)v=(v<<1)|bit(cur+k);return v;};
      if(rlen>0){uint32_t st=rd(TANS_L); for(int k=0;k<rlen;k++){byte_t sym=dec.symbol[st]; rec.push_back(sym);
          if(!ok_dbg && blocks==2 && sym!=in[chunkbase+k]){printf("  blk2 chunk %d: mismatch at k=%d (rlen=%d clen=%d tb=%ld): got %d want %d\n",c,k,rlen,clen,tb,sym,in[chunkbase+k]);ok_dbg=1;}
          int nb=dec.nbBits[st];if(k==rlen-1)break;uint32_t lo=rd(nb);st=dec.newState[st]+lo;}}
      chunkbase+=rlen; }
    if((int)rec.size()!=N||memcmp(rec.data(),in.data(),N)!=0){ok=0;printf("block %d FAIL (rec=%zu N=%d)\n",blocks,rec.size(),N);if(blocks>2)break;}
    blocks++;
  }
  printf("K=%d MAX_IN=%d round-trip over %d blocks: %s (ratio %.1f%%)\n",KWAY,MAX_IN,blocks,ok?"ALL LOSSLESS":"FAIL",100.0*to/ti);
  return ok?0:1;
}
