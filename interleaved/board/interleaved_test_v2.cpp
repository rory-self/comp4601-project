#include <stdio.h>
#include <string.h>
#include "arith_interleaved_v2.h"

#ifndef LANES
#define LANES 16
#endif

static long g_bit;
static const byte_t *g_in;
static int g_len;

static uint32_t get_bit() {
    int bi = (int)(g_bit >> 3), off = 7 - (int)(g_bit & 7);
    g_bit++;
    return bi < g_len ? (g_in[bi] >> off) & 1 : 0;
}

static int decode_bit(uint16_t &low, uint16_t &high, uint16_t &code,
                      uint16_t &prob) {
    uint32_t range = (uint32_t)high - low + 1u;
    uint32_t split = (range * prob) >> PROB_BITS;
    int bit;
    if ((uint16_t)(code - low) < split) {
        bit = 0;
        high = (uint16_t)(low + split - 1);
    } else {
        bit = 1;
        low = (uint16_t)(low + split);
    }
    for (;;) {
        if (high < HALF) {
        } else if (low >= HALF) {
            code -= HALF; low -= HALF; high -= HALF;
        } else if (low >= FIRST_QTR && high < THIRD_QTR) {
            code -= FIRST_QTR; low -= FIRST_QTR; high -= FIRST_QTR;
        } else break;
        low = (uint16_t)(low << 1);
        high = (uint16_t)((high << 1) | 1u);
        code = (uint16_t)((code << 1) | get_bit());
    }
    if (bit == 0) prob += (PROB_TOTAL - prob) >> MOVE_BITS;
    else prob -= prob >> MOVE_BITS;
    return bit;
}

static int decode_chunk(const byte_t *in, int len, byte_t *out) {
    g_in = in; g_len = len; g_bit = 0;
    uint16_t flag = PROB_INIT, tree[NTREE];
    for (int i = 0; i < NTREE; i++) tree[i] = PROB_INIT;
    uint16_t low = 0, high = TOP_VALUE, code = 0;
    for (int i = 0; i < CODE_BITS; i++) code = (code << 1) | get_bit();
    int oi = 0;
    while (decode_bit(low, high, code, flag)) {
        int context = 1, value = 0;
        for (int j = 7; j >= 0; j--) {
            int bit = decode_bit(low, high, code, tree[context]);
            value = (value << 1) | bit;
            context = (context << 1) | bit;
        }
        out[oi++] = (byte_t)value;
    }
    return oi;
}

static byte_t message[MAX_IN], encoded[MAX_OUT], decoded[MAX_IN];

static int run_case(const char *name, int n) {
    int encoded_len = arith_encode_interleaved(message, n, encoded);
    int offset = 2 * LANES, decoded_len = 0;
    for (int c = 0; c < LANES; c++) {
        int len = encoded[2*c] | (encoded[2*c+1] << 8);
        decoded_len += decode_chunk(encoded + offset, len,
                                    decoded + decoded_len);
        offset += len;
    }
    int ok = decoded_len == n && memcmp(message, decoded, n) == 0;
    printf("%-8s in=%4d coded=%4d ratio=%6.2f%% %s\n", name, n,
           encoded_len, n ? 100.0 * encoded_len / n : 0.0,
           ok ? "OK" : "FAIL");
    return ok;
}

int main() {
    int ok = 1;
    for (int i = 0; i < 4095; i++) message[i] = 'a' + i % 7;
    ok &= run_case("pattern", 4095);
    unsigned r = 7;
    for (int i = 0; i < 2048; i++) {
        r = r * 1103515245u + 12345u;
        message[i] = (r >> 16) & 0xff;
    }
    ok &= run_case("random", 2048);
    for (int i = 0; i < 10; i++) message[i] = 'x';
    ok &= run_case("tiny", 10);
    puts(ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
