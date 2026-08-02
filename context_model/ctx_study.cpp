// Higher-order / spatial context modelling.
// Everything so far modelled ONE byte in isolation (order-0 bit-tree). For images
// the redundancy is mostly SPATIAL -- a pixel resembles its left/above neighbours.
// This measures how much compression each context choice buys, and (crucially for
// hardware) what it COSTS in context-memory, which is what sets FPGA area.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
using namespace std;

static vector<uint8_t> rd_pgm(const string&p,int&W,int&H){
  ifstream f(p,ios::binary); string m; int mv; f>>m>>W>>H>>mv; f.get();
  vector<uint8_t> px((size_t)W*H); f.read((char*)px.data(),px.size()); return px; }

// Empirical conditional entropy H(sym | ctx) in bits/symbol, given a context id
// per symbol. Also reports how many distinct contexts were actually used.
static double cond_H(const vector<uint8_t>&d,const vector<int>&ctx,int nctx,int&used){
  vector<vector<uint32_t>> c(nctx); vector<uint32_t> tot(nctx,0);
  for(size_t i=0;i<d.size();i++){ int k=ctx[i]; if(c[k].empty())c[k].assign(256,0); c[k][d[i]]++; tot[k]++; }
  double bits=0; used=0;
  for(int k=0;k<nctx;k++){ if(!tot[k])continue; used++;
    for(int s=0;s<256;s++) if(c[k][s]) bits += c[k][s]*(-log2((double)c[k][s]/tot[k])); }
  return bits/d.size(); }

int main(int argc,char**argv){
  printf("%-16s %-28s %8s %9s %10s %s\n","image","context model","bits/sym","ratio","contexts","ctx memory");
  printf("---------------- ---------------------------- -------- --------- ---------- -----------\n");
  for(int a=1;a<argc;a++){
    int W,H; auto d=rd_pgm(argv[a],W,H); if(d.empty())continue; size_t N=d.size();
    string nm=argv[a]; auto sl=nm.find_last_of('/'); if(sl!=string::npos)nm=nm.substr(sl+1);
    struct M{const char*name;int nctx;vector<int>ctx;};
    vector<M> ms;
    { vector<int> c(N,0); ms.push_back({"order-0 (what we built)",1,c}); }
    { vector<int> c(N); for(size_t i=0;i<N;i++) c[i]= i? d[i-1]:0;                 ms.push_back({"order-1 (prev byte)",256,c}); }
    { vector<int> c(N); for(size_t i=0;i<N;i++) c[i]= (i>=(size_t)W)? d[i-W]:0;    ms.push_back({"spatial: above",256,c}); }
    { vector<int> c(N); for(size_t i=0;i<N;i++){ int L=i?d[i-1]:0, U=i>=(size_t)W?d[i-W]:0;
        c[i]=((L>>4)<<4)|(U>>4); }                                                 ms.push_back({"spatial: left+above (4b)",256,c}); }
    { vector<int> c(N); for(size_t i=0;i<N;i++){ int L=i?d[i-1]:0, U=i>=(size_t)W?d[i-W]:0,
        UL=(i>=(size_t)W+1)?d[i-W-1]:0; int grad=L+U-UL; if(grad<0)grad=0; if(grad>255)grad=255;
        c[i]=grad; }                                                               ms.push_back({"MED predictor (JPEG-LS)",256,c}); }
    for(auto&m:ms){ int used=0; double h=cond_H(d,m.ctx,m.nctx,used);
      printf("%-16s %-28s %8.3f %8.1f%% %10d %8.1f KB\n",nm.c_str(),m.name,h,100*h/8,used,used*256*1.5/1024.0);
      nm=""; }
  }
  printf("\nctx memory = contexts x 256 symbols x ~12-bit state: this is the FPGA cost.\n"
         "Our bit-tree coder needs 255 probability states PER context.\n");
  return 0;
}
