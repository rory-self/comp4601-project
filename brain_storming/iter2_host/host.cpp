/*
 * On-fabric host for the K-way arithmetic-coding kernel (KV260, XRT).
 *  - loads the xclbin, runs arith_kernel(in, n, out, out_len) on the PL
 *  - verifies losslessness (decode the board output on the host, compare to input)
 *  - profiles with std::chrono around ONLY the kernel enqueue+wait
 * Build: aarch64 cross-compile against the XRT sysroot.
 */
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <algorithm>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#define KWAY 8
#define MAX_IN 4096
#define MAX_OUT 8192
typedef unsigned char byte_t;

// ---- probability model constants (must match the kernel / arith3.h) ----
#define CODE_BITS 16
#define TOP_VALUE ((uint32_t)((1u<<CODE_BITS)-1))
#define FIRST_QTR ((TOP_VALUE+1)/4)
#define HALF (2*FIRST_QTR)
#define THIRD_QTR (3*FIRST_QTR)
#define PROB_BITS 12
#define PROB_TOTAL (1u<<PROB_BITS)
#define PROB_INIT (PROB_TOTAL/2)
#define MOVE_BITS 5
#define NTREE 256

// ---- host-side decoder to verify the board output is lossless ----
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

static std::string arg_of(int c,char**v,const char*f,const char*d){for(int i=1;i<c-1;i++)if(!strcmp(v[i],f))return v[i+1];return d;}

int main(int argc,char**argv){
    std::string xclbin=arg_of(argc,argv,"-x","arith.bin");
    int dev=std::stoi(arg_of(argc,argv,"-d","0"));
    int N=std::stoi(arg_of(argc,argv,"-N","4095"));   // symbols
    int iters=std::stoi(arg_of(argc,argv,"-n","2000"));
    if(N>MAX_IN)N=MAX_IN;

    std::cout<<"Open device "<<dev<<"\nLoad xclbin "<<xclbin<<"\n";
    auto device=xrt::device(dev);
    auto uuid=device.load_xclbin(xclbin);
    auto krnl=xrt::kernel(device,uuid,"arith_kernel");

    auto bo_in =xrt::bo(device,MAX_IN ,krnl.group_id(0));
    auto bo_out=xrt::bo(device,MAX_OUT,krnl.group_id(2));
    auto bo_len=xrt::bo(device,sizeof(int),krnl.group_id(3));
    auto hin =bo_in.map<byte_t*>();
    auto hout=bo_out.map<byte_t*>();
    auto hlen=bo_len.map<int*>();

    // input: compressible pattern (matches the co-sim workload)
    for(int i=0;i<N;i++) hin[i]=(byte_t)('a'+(i%7));
    bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // --- correctness run ---
    auto run=krnl(bo_in,N,bo_out,bo_len); run.wait();
    bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    int clen=hlen[0];
    static byte_t dec[MAX_IN];
    int dn=dec_kway(hout,clen,dec);
    bool ok=(dn==N); for(int i=0;i<N&&ok;i++) ok=(dec[i]==hin[i]);
    std::cout<<"----------------------------------------------\n";
    std::cout<<"input "<<N<<" symbols -> compressed "<<clen<<" bytes ("<<(100.0*clen/N)<<"%)\n";
    std::cout<<(ok?"PASS: board output is lossless (decode == input)\n":"FAIL: round-trip mismatch\n");
    std::cout<<"----------------------------------------------\n";

    // --- profiling: time only enqueue+wait ---
    std::vector<double> us; us.reserve(iters);
    for(int k=0;k<iters;k++){
        auto t0=std::chrono::high_resolution_clock::now();
        auto r=krnl(bo_in,N,bo_out,bo_len); r.wait();
        auto t1=std::chrono::high_resolution_clock::now();
        us.push_back(std::chrono::duration<double,std::micro>(t1-t0).count());
    }
    std::sort(us.begin(),us.end());
    double tot=0; for(double v:us) tot+=v;
    double mean=tot/us.size(), med=us[us.size()/2];
    std::cout<<"\n=== On-fabric kernel profiling (chrono around krnl()+wait only) ===\n";
    std::cout<<"iterations       : "<<iters<<"\n";
    std::cout<<"mean per call    : "<<mean<<" us  ("<<N<<" symbols)\n";
    std::cout<<"median per call  : "<<med<<" us\n";
    std::cout<<"min/max per call : "<<us.front()<<" / "<<us.back()<<" us\n";
    std::cout<<"per-symbol       : "<<(mean*1000.0/N)<<" ns/sym\n";
    std::cout<<"throughput       : "<<(N/mean)<<" M symbols/s\n";
    return ok?0:1;
}
