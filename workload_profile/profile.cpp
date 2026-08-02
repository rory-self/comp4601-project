// Workload profiler: measures the CLASSIFIERS that decide which hardware
// acceleration technique a file can use. One row per file.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
using namespace std;
static vector<uint8_t> rd(const string&p){ifstream f(p,ios::binary);f.seekg(0,ios::end);size_t n=f.tellg();f.seekg(0);
  string m;if(p.size()>4&&p.substr(p.size()-4)==".pgm"){int w,h,v;f>>m>>w>>h>>v;f.get();vector<uint8_t>px((size_t)w*h);f.read((char*)px.data(),px.size());return px;}
  vector<uint8_t>b(n);f.read((char*)b.data(),n);return b;}
int main(int argc,char**argv){
  printf("%-16s %7s %6s %6s %6s %7s %8s %s\n","file","bytes","H0","Hmax_bp","Hmin_bp","p_max","meanRun","bypass-able bitplanes (H>0.98)");
  printf("---------------- ------- ------ ------ ------ ------- -------- ----------------------------\n");
  for(int a=1;a<argc;a++){
    auto d=rd(argv[a]); size_t n=d.size(); if(!n)continue;
    long c[256]={0}; for(auto b:d)c[b]++;
    double H=0,pmax=0; for(int s=0;s<256;s++) if(c[s]){double p=(double)c[s]/n;H-=p*log2(p);pmax=max(pmax,p);}
    // per-bit-plane entropy
    double hbp[8]; int nbypass=0; string bp;
    for(int b=7;b>=0;b--){ long o=0; for(auto v:d) o+=(v>>b)&1; double p=(double)o/n;
      double h=(p<=0||p>=1)?0:-(p*log2(p)+(1-p)*log2(1-p)); hbp[b]=h;
      if(h>0.98){nbypass++; bp+=to_string(b)+" ";} }
    double hmax=*max_element(hbp,hbp+8), hmin=*min_element(hbp,hbp+8);
    // mean run length (identical consecutive bytes)
    long runs=1; for(size_t i=1;i<n;i++) if(d[i]!=d[i-1]) runs++;
    double meanRun=(double)n/runs;
    string nm=argv[a]; auto sl=nm.find_last_of('/'); if(sl!=string::npos)nm=nm.substr(sl+1);
    printf("%-16s %7zu %6.3f %6.3f %6.3f %7.4f %8.2f %d planes: %s\n",nm.c_str(),n,H,hmax,hmin,pmax,meanRun,nbypass,bp.c_str());
  }
  return 0;
}
