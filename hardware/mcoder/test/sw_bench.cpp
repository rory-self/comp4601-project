// Software throughput baseline: the same M-coder as the kernel, compiled for
// the CPU (build with -DMC_KWAY=1). Times the encoder with std::chrono.
//
// Deliberately mirrors replication_full/test/sw_bench.cpp -- same workload
// ('a'+(i%7)), same symbol count, same timing method -- so the two software
// baselines are directly comparable.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "mcoder.h"

int mc_encode(const mc_byte* in, int n, mc_byte* out);   // from mcoder_enc.cpp

namespace {
using Clock = std::chrono::high_resolution_clock;
constexpr int kSymbols = MC_MAX_IN - 1;
constexpr int kReps    = 20000;
}

int main() {
    std::array<mc_byte, MC_MAX_IN>  input{};
    std::array<mc_byte, MC_MAX_OUT> output{};
    for (int i = 0; i < kSymbols; ++i)
        input[i] = static_cast<mc_byte>('a' + (i % 7));

    int compressed = 0;
    const auto t0 = Clock::now();
    for (int k = 0; k < kReps; ++k)
        compressed = mc_encode(input.data(), kSymbols, output.data());
    const auto t1 = Clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    const double ns_per_sym = ns / (double)kReps / (double)kSymbols;

    printf("M-coder software baseline (MC_KWAY=%d)\n", MC_KWAY);
    printf("  n=%d symbols, comp=%d bytes (%.1f%%)\n",
           kSymbols, compressed, 100.0 * compressed / kSymbols);
    printf("  %.2f ns/symbol   =>   %.2f M symbols/s\n",
           ns_per_sym, 1000.0 / ns_per_sym);
    return 0;
}
