#ifndef V5_PROF_H_
#define V5_PROF_H_
#include <stdint.h>

/*
 * INSTRUMENTED COPY of best_hls/arith3.h + best_hls/arith5.cpp.
 *
 * Byte-for-byte the same arithmetic, with work counters added so the cycle
 * model in mcoder_test.cpp can compare V5 against the M-coder on the same
 * corpus.  Do not develop against this file -- best_hls/ is the source of
 * truth.  If best_hls changes, regenerate this.
 */

#define V5_MAX_IN    4096
#define V5_MAX_OUT   8192

#define V5_CODE_BITS   16
#define V5_TOP_VALUE   ((uint32_t)((1u << V5_CODE_BITS) - 1))
#define V5_FIRST_QTR   ((V5_TOP_VALUE + 1) / 4)
#define V5_HALF        (2 * V5_FIRST_QTR)
#define V5_THIRD_QTR   (3 * V5_FIRST_QTR)

#define V5_PROB_BITS   12
#define V5_PROB_TOTAL  (1u << V5_PROB_BITS)
#define V5_PROB_INIT   (V5_PROB_TOTAL / 2)
#define V5_MOVE_BITS   5
#define V5_NTREE       256

#ifndef V5_KWAY
#define V5_KWAY 4
#endif
#define V5_CHUNK_CAP  (V5_MAX_IN / V5_KWAY + 64)

typedef unsigned char v5_byte;

typedef struct {
    long bins;          /* binary decisions coded                           */
    long renorm_iters;  /* renorm loop iterations actually executed         */
    long emit_calls;    /* put_bit invocations                              */
    long emit_bits;     /* bits appended                                    */
    long pending_bits;  /* bits emitted by the while(pending) drain loops    */
    long mults;         /* variable-operand multiplies (range * prob)       */
} v5_prof_t;
extern v5_prof_t g_v5_prof;

int v5_encode(const v5_byte in[V5_MAX_IN], int n, v5_byte out[V5_MAX_OUT]);
int v5_decode(const v5_byte *comp, int comp_len, v5_byte *out);

#endif /* V5_PROF_H_ */
