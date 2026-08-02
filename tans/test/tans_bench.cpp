// tANS: round-trip correctness, then real compressed size (payload + table) on
// the images vs the adaptive coder (arith5). Confirms the de-risk with a REAL
// static table-driven implementation, not just an entropy floor.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include "tans.h"
#include "arith3.h"
using Clock = std::chrono::high_resolution_clock;
int arith_encode(const unsigned char* in, int n, unsigned char* out);   // arith5.cpp -DKWAY=1

static const int L = 12;   // tableLog: 4096 states

static bool roundtrip(const uint8_t* in, int N) {
    long cnt[256] = {0}; for (int i = 0; i < N; i++) cnt[in[i]]++;
    int norm[256] = {0};
    if (N > 0) tans::normalize(cnt, N, L, norm);
    tans::Enc e; tans::Dec d; tans::build_enc(e, norm, L); tans::build_dec(d, norm, L);
    tans::BitStack bs; tans::encode(e, in, N, bs);
    std::vector<uint8_t> out(N ? N : 1);
    tans::decode(d, bs, out.data(), N);
    return N == 0 || memcmp(in, out.data(), N) == 0;
}

static std::vector<uint8_t> read_pgm(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::string magic; int w, h, mv; f >> magic >> w >> h >> mv; f.get();
    std::vector<uint8_t> px((size_t)w * h);
    f.read(reinterpret_cast<char*>(px.data()), px.size());
    return px;
}

int main(int argc, char** argv) {
    // ---- correctness ----
    printf("== round-trip ==\n");
    { std::vector<uint8_t> v(4095); for (int i=0;i<4095;i++) v[i]='a'+i%7;
      printf("  pattern(4095): %s\n", roundtrip(v.data(),4095)?"LOSSLESS":"FAIL"); }
    { std::vector<uint8_t> v(2048); unsigned r=7; for (int i=0;i<2048;i++){r=r*1103515245u+12345u; v[i]=(r>>16)&0xff;}
      printf("  random(2048):  %s\n", roundtrip(v.data(),2048)?"LOSSLESS":"FAIL"); }
    { std::vector<uint8_t> v(300); for (int i=0;i<300;i++) v[i]=(i*37)&0xff;
      printf("  mixed(300):    %s\n", roundtrip(v.data(),300)?"LOSSLESS":"FAIL"); }
    { uint8_t v[10]; for (int i=0;i<10;i++) v[i]='x'; printf("  tiny(10):      %s\n", roundtrip(v,10)?"LOSSLESS":"FAIL"); }

    // ---- speed: the point of the tree method (pure table traversal, no arithmetic) ----
    // Scenario: a fixed shared table (built once, amortised), applied to each file.
    {
        const int N = 4096, REPS = 4000;
        std::vector<uint8_t> v(N); for (int i=0;i<N;i++) v[i]='a'+i%13;
        long cnt[256]={0}; for (int i=0;i<N;i++) cnt[v[i]]++;
        int norm[256]={0}; tans::normalize(cnt,N,L,norm);
        tans::Enc e; tans::build_enc(e,norm,L);              // table built ONCE
        std::vector<uint8_t> ob(MAX_OUT);
        auto t0=Clock::now();
        for (int r=0;r<REPS;r++){ tans::BitStack bs; tans::encode(e,v.data(),N,bs); }
        double tans_ns = std::chrono::duration<double,std::nano>(Clock::now()-t0).count()/((double)REPS*N);
        auto t1=Clock::now();
        for (int r=0;r<REPS;r++) arith_encode(v.data(),N,ob.data());
        double ar_ns = std::chrono::duration<double,std::nano>(Clock::now()-t1).count()/((double)REPS*N);
        printf("\n== encode speed (shared table, amortised), this CPU ==\n");
        printf("  tANS (table traversal, no arithmetic): %.2f ns/sym\n", tans_ns);
        printf("  arith5 (adaptive, multiply per bin)  : %.2f ns/sym   -> tANS is %.2fx faster\n",
               ar_ns, ar_ns/tans_ns);
    }

    if (argc < 2) return 0;
    // ---- real size on images vs adaptive ----
    printf("\n== tANS (static, whole-image table) vs adaptive (arith5, 4KB blocks) ==\n");
    printf("%-16s | %7s | %10s | %10s | %8s | %s\n","image","bytes","tANS+tbl","adaptive","delta","lossless");
    printf("-----------------+---------+------------+------------+----------+---------\n");
    for (int a = 1; a < argc; a++) {
        auto px = read_pgm(argv[a]); int N = (int)px.size(); if (!N) continue;
        long cnt[256]={0}; for (int i=0;i<N;i++) cnt[px[i]]++;
        int nsym=0; for (int s=0;s<256;s++) if (cnt[s]) nsym++;
        int norm[256]={0}; tans::normalize(cnt,N,L,norm);
        tans::Enc e; tans::Dec d; tans::build_enc(e,norm,L); tans::build_dec(d,norm,L);
        tans::BitStack bs; long payload_bits = tans::encode(e, px.data(), N, bs);
        std::vector<uint8_t> rec(N); tans::decode(d, bs, rec.data(), N);
        bool ok = memcmp(px.data(), rec.data(), N)==0;
        long table_bytes = (long)std::ceil(nsym*1.5)+8;
        long tans_bytes = (payload_bits+7)/8 + table_bytes;

        std::vector<uint8_t> ob(MAX_OUT); long adaptive=0;
        for (int off=0; off<N; off+=MAX_IN){ int m=std::min(MAX_IN,N-off); adaptive+=arith_encode(px.data()+off,m,ob.data()); }

        std::string name=argv[a]; auto sl=name.find_last_of('/'); if(sl!=std::string::npos) name=name.substr(sl+1);
        printf("%-16s | %7d | %10ld | %10ld | %+7.2f%% | %s\n", name.c_str(), N,
               tans_bytes, adaptive, 100.0*(tans_bytes-adaptive)/adaptive, ok?"YES":"NO!!");
    }
    return 0;
}
