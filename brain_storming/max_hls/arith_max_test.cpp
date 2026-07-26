#include <stdio.h>
#include <string.h>
#include "arith_max.h"

#ifndef KWAY
#define KWAY 32
#endif

static long bit_index;
static const byte_t *bit_input;
static int bit_length;

static uint32_t get_bit() {
    int byte_index = (int)(bit_index >> 3);
    int offset = 7 - (int)(bit_index & 7);
    bit_index++;
    return byte_index < bit_length ? (bit_input[byte_index] >> offset) & 1 : 0;
}

static int decode_bit(uint16_t &low, uint16_t &high, uint16_t &code,
                      uint16_t &prob) {
    uint32_t range = (uint32_t)high - low + 1;
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
        } else {
            break;
        }
        low = (uint16_t)(low << 1);
        high = (uint16_t)((high << 1) | 1);
        code = (uint16_t)((code << 1) | get_bit());
    }
    if (bit == 0) prob += (PROB_TOTAL - prob) >> MOVE_BITS;
    else prob -= prob >> MOVE_BITS;
    return bit;
}

static int decode_chunk(const byte_t *in, int len, byte_t *out) {
    bit_input = in; bit_length = len; bit_index = 0;
    uint16_t flag = PROB_INIT, probs[8];
    for (int i = 0; i < 8; i++) probs[i] = PROB_INIT;
    uint16_t low = 0, high = TOP_VALUE, code = 0;
    for (int i = 0; i < CODE_BITS; i++) code = (code << 1) | get_bit();
    int oi = 0;
    while (decode_bit(low, high, code, flag)) {
        int value = 0;
        for (int i = 0; i < 8; i++)
            value = (value << 1) | decode_bit(low, high, code, probs[i]);
        out[oi++] = (byte_t)value;
    }
    return oi;
}

static byte_t encoded[MAX_OUT], decoded[MAX_IN], message[MAX_IN];

static int run_case(const char *name, int n) {
    int coded = arith_encode_max(message, n, encoded);
    int offset = 2 * KWAY, decoded_len = 0;
    for (int c = 0; c < KWAY; c++) {
        int len = encoded[2*c] | (encoded[2*c+1] << 8);
        decoded_len += decode_chunk(encoded + offset, len,
                                    decoded + decoded_len);
        offset += len;
    }
    int ok = decoded_len == n && memcmp(message, decoded, n) == 0;
    printf("%-10s in=%4d coded=%4d ratio=%6.2f%% %s\n", name, n, coded,
           n ? 100.0 * coded / n : 0.0, ok ? "OK" : "FAIL");
    return ok;
}

int main() {
    int ok = 1;
    for (int i = 0; i < MAX_IN - 1; i++) message[i] = 'a' + i % 7;
    ok &= run_case("pattern", MAX_IN - 1);
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

