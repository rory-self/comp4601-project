// Software throughput baseline: the same binary arithmetic coder as best_hls,
// compiled for the CPU (build with -DKWAY=1). Times the encoder with std::chrono.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "arith3.h"

int arith_encode(const byte_t* in, int n, byte_t* out);   // from arith5.cpp

namespace {
using Clock = std::chrono::high_resolution_clock;
constexpr int kSymbols = MAX_IN - 1;
constexpr int kReps    = 20000;
}

int main() {
    // Compressible workload (matches the co-simulation / on-board input).
    std::array<byte_t, MAX_IN>  input{};
    std::array<byte_t, MAX_OUT> output{};
    for (int i = 0; i < kSymbols; ++i)
        input[i] = static_cast<byte_t>('a' + (i % 7));

    int compressed = 0;
    const auto t0 = Clock::now();
    for (int k = 0; k < kReps; ++k)
        compressed = arith_encode(input.data(), kSymbols, output.data());
    const auto t1 = Clock::now();

    const double us_per_call = std::chrono::duration<double, std::micro>(t1 - t0).count() / kReps;
    const double ns_per_sym  = us_per_call * 1000.0 / kSymbols;

    std::printf("n=%d symbols, comp=%d bytes (%.1f%%)\n",
                kSymbols, compressed, 100.0 * compressed / kSymbols);
    std::printf("SW encode: %.2f us/message, %.2f ns/symbol, %.2f M symbols/s\n",
                us_per_call, ns_per_sym, 1000.0 / ns_per_sym);
    return 0;
}
