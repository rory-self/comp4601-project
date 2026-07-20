/*
 * Testbench: encode with the kernel, decode with a SW reference decoder,
 * verify lossless round-trip. This doubles as the HLS C-simulation bench.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "arith.h"

// -------- SW reference decoder (mirror of the encoder's model/renorm) --------
static int arith_decode(const byte_t *in, int in_len, byte_t *out) {
    uint32_t freq[NSYM], cum[NSYM + 1];
    for (int i = 0; i < NSYM; i++) freq[i] = 1;

    long bit_idx = 0;
    // MSB-first bit reader; reads 0 past end (matches encoder padding).
    #define GET_BIT() ({                                   \
        int _bi = (int)(bit_idx >> 3);                     \
        int _off = 7 - (int)(bit_idx & 7);                 \
        bit_idx++;                                         \
        (uint32_t)((_bi < in_len) ? ((in[_bi] >> _off) & 1) : 0); })

    uint32_t low = 0, high = TOP_VALUE, code = 0;
    for (int i = 0; i < CODE_BITS; i++) code = (code << 1) | GET_BIT();

    int out_n = 0;
    for (;;) {
        uint32_t total = 0;
        for (int i = 0; i < NSYM; i++) { cum[i] = total; total += freq[i]; }
        cum[NSYM] = total;

        uint32_t range = high - low + 1;
        uint32_t value = (uint32_t)((((uint64_t)(code - low) + 1) * total - 1) / range);

        int s = 0;
        while (cum[s + 1] <= value) s++;

        uint32_t cumLow = cum[s], cumHigh = cum[s + 1];
        high = low + (uint32_t)(((uint64_t)range * cumHigh) / total) - 1;
        low  = low + (uint32_t)(((uint64_t)range * cumLow ) / total);

        for (;;) {
            if (high < HALF) {
            } else if (low >= HALF) {
                code -= HALF; low -= HALF; high -= HALF;
            } else if (low >= FIRST_QTR && high < THIRD_QTR) {
                code -= FIRST_QTR; low -= FIRST_QTR; high -= FIRST_QTR;
            } else break;
            low  = (low << 1) & TOP_VALUE;
            high = ((high << 1) | 1) & TOP_VALUE;
            code = ((code << 1) | GET_BIT()) & TOP_VALUE;
        }

        if (s == EOF_SYM) break;
        out[out_n++] = (byte_t)s;

        freq[s]++;
        if (total + 1 >= MAX_FREQ)
            for (int i = 0; i < NSYM; i++) freq[i] = (freq[i] + 1) >> 1;
    }
    return out_n;
}

static byte_t enc_out[MAX_OUT];
static byte_t dec_out[MAX_IN];

static int run_case(const char *name, const byte_t *msg, int n) {
    int comp_len = 0;
    arith_encode(msg, n, enc_out, &comp_len);
    int dec_n = arith_decode(enc_out, comp_len, dec_out);

    int ok = (dec_n == n) && (memcmp(msg, dec_out, n) == 0);
    printf("%-18s in=%4d bytes  comp=%4d bytes  ratio=%5.2f%%  round-trip=%s\n",
           name, n, comp_len, 100.0 * comp_len / (n ? n : 1), ok ? "OK" : "FAIL");
    return ok;
}

int main() {
    int all_ok = 1;

    // Case 1: highly repetitive (should compress well)
    {
        byte_t m[512];
        for (int i = 0; i < 512; i++) m[i] = 'A' + (i % 3);
        all_ok &= run_case("repetitive", m, 512);
    }
    // Case 2: English-ish text
    {
        const char *t = "the quick brown fox jumps over the lazy dog. "
                        "the quick brown fox jumps over the lazy dog. "
                        "arithmetic coding compresses toward entropy.";
        all_ok &= run_case("text", (const byte_t *)t, (int)strlen(t));
    }
    // Case 3: pseudo-random bytes (near-incompressible)
    {
        byte_t m[1024];
        unsigned r = 12345;
        for (int i = 0; i < 1024; i++) { r = r * 1103515245u + 12345u; m[i] = (r >> 16) & 0xFF; }
        all_ok &= run_case("random", m, 1024);
    }
    // Case 4: single byte + empty edge cases
    {
        byte_t m[1] = { 0x42 };
        all_ok &= run_case("single-byte", m, 1);
        all_ok &= run_case("empty", m, 0);
    }

    printf("----------------------------------------------\n");
    if (all_ok) { printf("PASS: all round-trips lossless\n"); return 0; }
    else        { printf("FAIL: a round-trip mismatched\n");  return 1; }
}
