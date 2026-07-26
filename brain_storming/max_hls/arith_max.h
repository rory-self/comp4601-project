#ifndef ARITH_MAX_H_
#define ARITH_MAX_H_

#include <stdint.h>

#define MAX_IN 4096
#define MAX_OUT 8192

#define CODE_BITS 16
#define TOP_VALUE ((uint16_t)0xffffu)
#define FIRST_QTR ((uint16_t)0x4000u)
#define HALF ((uint16_t)0x8000u)
#define THIRD_QTR ((uint16_t)0xc000u)

#define PROB_BITS 12
#define PROB_TOTAL 4096u
#define PROB_INIT 2048u
#define MOVE_BITS 5

typedef unsigned char byte_t;

int arith_encode_max(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT]);

#endif

