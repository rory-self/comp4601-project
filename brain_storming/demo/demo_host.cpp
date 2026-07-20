/*
 * Visual demo: compress + decompress a grayscale image, CPU vs FPGA, on the board.
 *  - loads a P5 (.pgm) image
 *  - compresses it block-by-block (4 KB blocks) with (a) the ARM CPU coder and
 *    (b) the FPGA kernel, timing each
 *  - decompresses (CPU) and checks the reconstruction is pixel-perfect (lossless)
 *  - prints an ASCII preview + a stats table, and writes reconstructed.pgm
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#define KWAY 8
#define MAX_IN 4096
#define MAX_OUT 8192
typedef unsigned char byte_t;
int arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);   // CPU coder (arith5.cpp)

// ---- probability-model constants (match the coder) ----
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

// ---- CPU decoder (to reconstruct) ----
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

// ---- ASCII preview of an image ----
static void ascii_preview(const std::vector<byte_t>&px,int W,int H,const char*title){
    const char*ramp=" .:-=+*#%@";
    int cols=64, rows=cols*H/W/2; if(rows<1)rows=1;
    std::cout<<title<<":\n";
    for(int r=0;r<rows;r++){ std::string line;
        for(int c=0;c<cols;c++){int x=c*W/cols,y=r*H/rows; int v=px[y*W+x]; line+=ramp[v*9/255];}
        std::cout<<"  "<<line<<"\n"; }
}

int main(int argc,char**argv){
    std::string img="image.pgm", xclbin="arith.bin";
    for(int i=1;i<argc-1;i++){ if(!strcmp(argv[i],"-i"))img=argv[i+1]; if(!strcmp(argv[i],"-x"))xclbin=argv[i+1]; }

    // ---- read P5 PGM ----
    std::ifstream f(img,std::ios::binary);
    if(!f){std::cerr<<"cannot open "<<img<<"\n";return 1;}
    std::string magic; int W,H,maxv; f>>magic>>W>>H>>maxv; f.get();
    if(magic!="P5"){std::cerr<<"need a binary P5 PGM\n";return 1;}
    std::vector<byte_t> px(W*H); f.read((char*)px.data(),W*H); f.close();
    int total=W*H, nblk=(total+MAX_IN-1)/MAX_IN;

    std::cout<<"================ IMAGE COMPRESSION DEMO ================\n";
    std::cout<<"image: "<<img<<"  "<<W<<"x"<<H<<" = "<<total<<" bytes, "<<nblk<<" blocks of <="<<MAX_IN<<"\n\n";
    ascii_preview(px,W,H,"ORIGINAL");

    // ---- FPGA setup ----
    auto device=xrt::device(0);
    auto uuid=device.load_xclbin(xclbin);
    auto krnl=xrt::kernel(device,uuid,"arith_kernel");
    auto bo_in=xrt::bo(device,MAX_IN,krnl.group_id(0));
    auto bo_out=xrt::bo(device,MAX_OUT,krnl.group_id(2));
    auto bo_len=xrt::bo(device,sizeof(int),krnl.group_id(3));
    auto hin=bo_in.map<byte_t*>(); auto hout=bo_out.map<byte_t*>(); auto hlen=bo_len.map<int*>();

    // per-block compressed store + lengths (for reconstruction)
    std::vector<std::vector<byte_t>> comp_cpu(nblk), comp_fpga(nblk);
    static byte_t obuf[MAX_OUT];

    // ---- CPU compress (timed) ----
    long comp_bytes=0;
    auto tc0=std::chrono::high_resolution_clock::now();
    for(int b=0;b<nblk;b++){int off=b*MAX_IN,n=std::min(MAX_IN,total-off);
        int cl=arith_encode(px.data()+off,n,obuf);
        comp_cpu[b].assign(obuf,obuf+cl); }
    auto tc1=std::chrono::high_resolution_clock::now();
    double cpu_us=std::chrono::duration<double,std::micro>(tc1-tc0).count();
    for(auto&v:comp_cpu) comp_bytes+=v.size();

    // ---- FPGA compress (timed) ----
    auto tf0=std::chrono::high_resolution_clock::now();
    for(int b=0;b<nblk;b++){int off=b*MAX_IN,n=std::min(MAX_IN,total-off);
        memcpy(hin,px.data()+off,n); bo_in.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        auto r=krnl(bo_in,n,bo_out,bo_len); r.wait();
        bo_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE); bo_len.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        int cl=hlen[0]; comp_fpga[b].assign(hout,hout+cl); }
    auto tf1=std::chrono::high_resolution_clock::now();
    double fpga_us=std::chrono::duration<double,std::micro>(tf1-tf0).count();

    // ---- reconstruct from FPGA-compressed data (CPU decode) ----
    std::vector<byte_t> recon(total); static byte_t dblk[MAX_IN];
    int rp=0;
    for(int b=0;b<nblk;b++){
        int dn=dec_kway(comp_fpga[b].data(),(int)comp_fpga[b].size(),dblk);
        memcpy(recon.data()+rp,dblk,dn); rp+=dn; }

    bool lossless = (rp==total) && (memcmp(recon.data(),px.data(),total)==0);
    // FPGA vs CPU produce identical compressed bytes?
    bool same=true; for(int b=0;b<nblk&&same;b++) same=(comp_cpu[b]==comp_fpga[b]);

    std::ofstream of("reconstructed.pgm",std::ios::binary);
    of<<"P5\n"<<W<<" "<<H<<"\n255\n"; of.write((char*)recon.data(),total); of.close();

    std::cout<<"\n";
    ascii_preview(recon,W,H,"RECONSTRUCTED (from FPGA-compressed bytes)");

    std::cout<<"\n===================== RESULTS =====================\n";
    std::cout<<"original size      : "<<total<<" bytes\n";
    std::cout<<"compressed size    : "<<comp_bytes<<" bytes  ("<<(100.0*comp_bytes/total)<<"% of original)\n";
    std::cout<<"lossless           : "<<(lossless?"YES  (reconstructed == original, pixel-perfect)":"NO")<<"\n";
    std::cout<<"CPU vs FPGA output : "<<(same?"identical bytes":"differ")<<"\n";
    std::cout<<"---------------------------------------------------\n";
    std::cout<<"ARM CPU  compress  : "<<cpu_us<<" us  ("<<(total/cpu_us)<<" MB/s)\n";
    std::cout<<"FPGA     compress  : "<<fpga_us<<" us  ("<<(total/fpga_us)<<" MB/s)\n";
    std::cout<<"SPEEDUP (FPGA/CPU) : "<<(cpu_us/fpga_us)<<"x\n";
    std::cout<<"===================================================\n";
    std::cout<<"wrote reconstructed.pgm\n";
    return lossless?0:1;
}
