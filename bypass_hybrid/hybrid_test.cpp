#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include "mcoder.h"
int hybrid_encode(const mc_byte*,int,mc_byte*); int hybrid_decode(const mc_byte*,int,mc_byte*);
#ifndef ADAPT_MASK
#define ADAPT_MASK 0xFF
#endif
static std::vector<unsigned char> rd(const std::string&p){std::ifstream f(p,std::ios::binary);
 if(p.size()>4&&p.substr(p.size()-4)==".pgm"){std::string m;int w,h,v;f>>m>>w>>h>>v;f.get();std::vector<unsigned char>px((size_t)w*h);f.read((char*)px.data(),px.size());return px;}
 f.seekg(0,std::ios::end);size_t n=f.tellg();f.seekg(0);std::vector<unsigned char>b(n);f.read((char*)b.data(),n);return b;}
int main(int argc,char**argv){
  int nad=0; for(int l=0;l<8;l++) nad+=(ADAPT_MASK>>l)&1;
  printf("ADAPT_MASK=0x%02X  adaptive bins/byte=%d  (plain=8) -> predicted %.2fx fewer coder cycles\n",ADAPT_MASK,nad,8.0/(nad?nad:1));
  for(int a=1;a<argc;a++){
    auto d=rd(argv[a]); long ti=0,to=0; bool ok=true;
    std::vector<mc_byte> ob(8192),rec(4096);
    for(size_t off=0;off<d.size();off+=4095){
      int N=(int)std::min((size_t)4095,d.size()-off);
      int cl=hybrid_encode(d.data()+off,N,ob.data());
      int dn=hybrid_decode(ob.data(),cl,rec.data());
      if(dn!=N||memcmp(rec.data(),d.data()+off,N)!=0){ok=false;break;}
      ti+=N; to+=cl;
    }
    std::string nm=argv[a];auto s=nm.find_last_of('/');if(s!=std::string::npos)nm=nm.substr(s+1);
    printf("  %-16s %s  ratio=%.1f%%\n",nm.c_str(),ok?"LOSSLESS":"FAIL",100.0*to/ti);
  }
  return 0;
}
