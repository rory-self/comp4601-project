// Static order-0 tANS (table ANS / "the tree method"): the multiply-free,
// table-driven entropy coder. Build one table from a fixed histogram, then
// encode a whole byte per state transition (state = table[state]) -- no multiply.
// This is a from-scratch FSE-style implementation, used to MEASURE the real
// compressed size (table included) of the static-model idea. For clarity of
// correctness the bit stream is a simple LIFO bit-stack (ANS is naturally LIFO);
// size is reported in bits, so packing details don't affect the measurement.
#ifndef TANS_H_
#define TANS_H_
#include <cstdint>
#include <vector>
#include <cstring>

namespace tans {

inline int highbit(uint32_t x) { int r = 0; while (x >>= 1) r++; return r; }  // floor(log2)

// Normalise raw counts to sum == (1<<L), every present symbol >= 1.
inline void normalize(const long count[256], long N, int L, int norm[256]) {
    const int tableSize = 1 << L;
    long distributed = 0; int largest = 0; int largestN = -1;
    const double scale = (double)tableSize / (double)N;
    for (int s = 0; s < 256; s++) {
        if (count[s] == 0) { norm[s] = 0; continue; }
        int p = (int)(count[s] * scale);
        if (p < 1) p = 1;
        norm[s] = p; distributed += p;
        if (norm[s] > largestN) { largestN = norm[s]; largest = s; }
    }
    norm[largest] += (int)(tableSize - distributed);   // absorb rounding in the largest
}

struct Enc {
    int L, tableSize;
    std::vector<uint16_t> stateTable;
    int      deltaFindState[256];
    uint32_t deltaNbBits[256];
};
struct Dec {
    int L, tableSize;
    std::vector<uint8_t>  symbol;
    std::vector<uint8_t>  nbBits;
    std::vector<uint16_t> newState;
};

inline void spread(const int norm[256], int tableSize, std::vector<uint8_t>& sym) {
    sym.resize(tableSize);
    const int step = (tableSize >> 1) + (tableSize >> 3) + 3;
    int pos = 0;
    for (int s = 0; s < 256; s++)
        for (int i = 0; i < norm[s]; i++) { sym[pos] = (uint8_t)s; pos = (pos + step) & (tableSize - 1); }
}

inline void build_enc(Enc& e, const int norm[256], int L) {
    e.L = L; e.tableSize = 1 << L;
    std::vector<uint8_t> sym; spread(norm, e.tableSize, sym);
    int cumul[257]; cumul[0] = 0;
    for (int s = 0; s < 256; s++) cumul[s + 1] = cumul[s] + norm[s];
    e.stateTable.assign(e.tableSize, 0);
    { int ct[256]; for (int s = 0; s < 256; s++) ct[s] = cumul[s];
      for (int i = 0; i < e.tableSize; i++) { int s = sym[i]; e.stateTable[ct[s]++] = (uint16_t)(e.tableSize + i); } }
    int total = 0;
    for (int s = 0; s < 256; s++) {
        if (norm[s] == 0) { e.deltaNbBits[s] = 0; e.deltaFindState[s] = 0; continue; }
        if (norm[s] == 1) {
            e.deltaNbBits[s]    = ((uint32_t)L << 16) - (1u << L);
            e.deltaFindState[s] = total - 1;
        } else {
            int maxBits = L - highbit((uint32_t)(norm[s] - 1));
            int minStatePlus = norm[s] << maxBits;
            e.deltaNbBits[s]    = ((uint32_t)maxBits << 16) - (uint32_t)minStatePlus;
            e.deltaFindState[s] = total - norm[s];
        }
        total += norm[s];
    }
}

inline void build_dec(Dec& d, const int norm[256], int L) {
    d.L = L; d.tableSize = 1 << L;
    std::vector<uint8_t> sym; spread(norm, d.tableSize, sym);
    d.symbol.assign(d.tableSize, 0); d.nbBits.assign(d.tableSize, 0); d.newState.assign(d.tableSize, 0);
    int next[256]; for (int s = 0; s < 256; s++) next[s] = norm[s];
    for (int i = 0; i < d.tableSize; i++) {
        int s = sym[i]; d.symbol[i] = (uint8_t)s;
        int x = next[s]++;
        int nb = d.L - highbit((uint32_t)x);
        d.nbBits[i]   = (uint8_t)nb;
        d.newState[i] = (uint16_t)((x << nb) - d.tableSize);
    }
}

// LIFO bit-stack. push appends, read pops (ANS reads what was written last).
struct BitStack {
    std::vector<uint8_t> b;
    void push(uint32_t v, int n) { for (int k = 0; k < n; k++) b.push_back((v >> k) & 1u); }
    uint32_t read(int n) { uint32_t r = 0; for (int k = 0; k < n; k++) { r = (r << 1) | b.back(); b.pop_back(); } return r; }
    long bits() const { return (long)b.size(); }
};

// Returns compressed size in BITS (payload only; add the table separately).
inline long encode(const Enc& e, const uint8_t* in, int N, BitStack& bs) {
    if (N == 0) return 0;
    // init state from the last symbol (FSE_initCState2)
    uint32_t s0 = in[N - 1];
    uint32_t nb = (e.deltaNbBits[s0] + (1u << 15)) >> 16;
    uint32_t state = (nb << 16) - e.deltaNbBits[s0];
    state = e.stateTable[(state >> nb) + e.deltaFindState[s0]];
    for (int i = N - 2; i >= 0; i--) {
        uint32_t s = in[i];
        uint32_t nbo = (state + e.deltaNbBits[s]) >> 16;
        bs.push(state & ((1u << nbo) - 1u), nbo);
        state = e.stateTable[(state >> nbo) + e.deltaFindState[s]];
    }
    bs.push(state & (e.tableSize - 1), e.L);   // flush final state
    return bs.bits();
}

inline void decode(const Dec& d, BitStack& bs, uint8_t* out, int N) {
    if (N == 0) return;
    uint32_t state = bs.read(d.L);
    for (int k = 0; k < N; k++) {
        out[k] = d.symbol[state];
        int nb = d.nbBits[state];
        uint32_t low = bs.read(nb);
        state = d.newState[state] + low;
    }
}

} // namespace tans
#endif
