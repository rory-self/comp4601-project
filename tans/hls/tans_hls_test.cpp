// Round-trip test for the HLS tANS encoder (arith_tans_hls.cpp): encode with the
// baked static table, decode backward (ANS is LIFO), verify lossless.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "tans.h"          // build_dec, normalize, Dec
#include "tans_table.h"    // TANS_L etc. (same table the kernel bakes in)

typedef unsigned char byte_t;
int tans_encode(const byte_t in[4096], int n, byte_t out[8192]);   // arith_tans_hls.cpp

static std::vector<uint8_t> read_pgm(const std::string& p){
    std::ifstream f(p,std::ios::binary); std::string m;int w,h,mv; f>>m>>w>>h>>mv; f.get();
    std::vector<uint8_t> px((size_t)w*h); f.read((char*)px.data(),px.size()); return px;
}

int main(int argc,char**argv){
    // rebuild the SAME norm the table was generated from (image.pgm histogram)
    auto px = read_pgm(argc>1?argv[1]:"../../replication_full/demo/image.pgm");
    long cnt[256]={0}; for(auto b:px) cnt[b]++;
    int norm[256]={0}; tans::normalize(cnt,px.size(),TANS_L,norm);
    tans::Dec dec; tans::build_dec(dec,norm,TANS_L);

    int ok=1, blocks=0;
    for(size_t off=0; off<px.size(); off+=4095){
        int N=(int)std::min((size_t)4095,px.size()-off);
        std::vector<byte_t> in(px.begin()+off,px.begin()+off+N), out(8192), rec(N);
        int clen = tans_encode(in.data(),N,out.data());
        long total_bits = out[0] | (out[1]<<8);
        // backward decode
        auto bit=[&](long j){ return (out[2+(j>>3)]>>(7-(j&7)))&1; };
        long cur=total_bits;
        auto rd=[&](int nb){ cur-=nb; uint32_t v=0; for(int k=0;k<nb;k++) v=(v<<1)|bit(cur+k); return v; };
        uint32_t state = rd(TANS_L);
        for(int k=0;k<N;k++){
            rec[k]=dec.symbol[state];
            int nb=dec.nbBits[state];
            if(k==N-1) break;                 // last symbol determined by state; its
            uint32_t low=rd(nb);              // trailing bits were never emitted (init)
            state=dec.newState[state]+low;
        }
        bool good = (rec==in);
        if(!good && blocks==0){
            // debug: compare to the proven software tans.h encode bit count
            tans::Enc se; tans::build_enc(se,norm,TANS_L);
            tans::BitStack bs; long swbits=tans::encode(se,in.data(),N,bs);
            printf("DEBUG blk0: HLS total_bits=%ld  sw bits=%ld  clen=%d\n",total_bits,swbits,clen);
            printf("  in [0..7]:  "); for(int k=0;k<8;k++) printf("%3d ",in[k]); printf("\n");
            printf("  rec[0..7]:  "); for(int k=0;k<8;k++) printf("%3d ",rec[k]); printf("\n");
            printf("  in [N-3..]: %d %d %d   rec: %d %d %d\n",in[N-3],in[N-2],in[N-1],rec[N-3],rec[N-2],rec[N-1]);
        }
        if(!good){ ok=0; printf("block %d (off %zu, N=%d) FAIL  clen=%d cur_left=%ld\n",blocks,off,N,clen,cur); if(blocks>2) break; }
        blocks++;
    }
    printf("tANS HLS round-trip over %d blocks: %s\n", blocks, ok?"ALL LOSSLESS":"FAIL");
    return ok?0:1;
}
