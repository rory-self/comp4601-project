#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include "arith3.h"
static byte_t in[MAX_IN], out[MAX_OUT];
int main(){
    // ~4000-symbol text-like payload (skewed distribution => realistic model work)
    
    
    int n=MAX_IN-1;
    for(int i=0;i<n;i++){ in[i]=(byte_t)('a'+(i%7)); }  // same workload as HW cosim
    int reps=20000, comp=0;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int k=0;k<reps;k++) comp=arith_encode(in,n,out);
    auto t1=std::chrono::high_resolution_clock::now();
    double us=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count()/1e3;
    double us_per_call=us/reps;
    double ns_per_sym=us_per_call*1e3/n;
    printf("n=%d symbols, comp=%d bytes (%.1f%%)\n", n, comp, 100.0*comp/n);
    printf("SW encode: %.2f us/message, %.2f ns/symbol, %.2f M symbols/s\n",
           us_per_call, ns_per_sym, 1e3/ns_per_sym);
    return 0;
}
