#ifndef ARITH_H_
#define ARITH_H_

#include <stdint.h>

// Alphabet: 256 byte values + 1 EOF symbol
#define NSYM      257
#define EOF_SYM   256

#define MAX_IN    4096
#define MAX_OUT   8192

// Classic Witten-Neal-Cleary 16-bit code values.
#define CODE_BITS   16
#define TOP_VALUE   ((uint32_t)((1u << CODE_BITS) - 1))  // 0xFFFF
#define FIRST_QTR   ((TOP_VALUE + 1) / 4)                // 0x4000
#define HALF        (2 * FIRST_QTR)                      // 0x8000
#define THIRD_QTR   (3 * FIRST_QTR)                      // 0xC000
#define MAX_FREQ    ((uint32_t)((1u << 14) - 1))         // 16383

typedef unsigned char byte_t;

// Top-level kernel: adaptive arithmetic encode of n input bytes.
//   in  : n input symbols (bytes)
//   out : packed compressed bytes
//   out_len : number of bytes written to out
void arith_encode(const byte_t in[MAX_IN], int n, byte_t out[MAX_OUT], int *out_len);

#endif
