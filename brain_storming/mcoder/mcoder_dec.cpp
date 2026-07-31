/*
 * M-coder decoder.  Host/verification side only -- this never goes into the
 * kernel, it exists to prove the encoder is lossless.
 *
 * Mirrors the encoder exactly: same tables, same context layout, same
 * initialisation.  Because the raw length is carried in the header, the
 * decoder knows to stop after 8*n bins and never needs a terminate bin.
 */
#include "mcoder.h"

int mc_decode_chunk(const mc_byte *in, int len, int n, mc_byte *out) {
    mc_ctx tree[MC_NTREE];
    for (int i = 0; i < MC_NTREE; i++) tree[i] = MC_CTX_INIT;
#ifdef MC_TERM_FLAG
    mc_ctx flag = MC_CTX_INIT;
    (void)n;
#endif

    mc_dec d;
    mc_dec_init(&d, in, len);

    int k = 0;
#ifdef MC_TERM_FLAG
    while (mc_decode_bin(&d, &flag)) {
#else
    for (; k < n; k++) {
#endif
        int b = 0, ctx = 1;
        for (int j = 7; j >= 0; j--) {
            int bit = mc_decode_bin(&d, &tree[ctx]);
            b   = (b << 1) | bit;
            ctx = (ctx << 1) | bit;
        }
        out[k] = (mc_byte)b;
#ifdef MC_TERM_FLAG
        k++;
#endif
    }
    return k;
}

int mc_decode(const mc_byte *comp, int comp_len, mc_byte *out) {
    if (comp_len < MC_HDR_BYTES) return -1;

    /*
     * The header is untrusted here.  Inside the test harness it always came
     * from our own encoder, but the XRT host reads it out of device memory, so
     * a wrong xclbin -- V5's, say -- yields an arbitrary header.  rlen drives
     * how many bytes get written to `out`, so it has to be validated before
     * use or a bogus header overruns the caller's MC_MAX_IN buffer.
     */
    int rlen[MC_KWAY], clen[MC_KWAY];
    long total_raw = 0;
    for (int c = 0; c < MC_KWAY; c++) {
        rlen[c] =  comp[4 * c + 0] | (comp[4 * c + 1] << 8);
        clen[c] =  comp[4 * c + 2] | (comp[4 * c + 3] << 8);
        if (rlen[c] < 0 || clen[c] < 0) return -1;
        total_raw += rlen[c];
    }
    if (total_raw > MC_MAX_IN) return -1;   /* would overrun `out` */

    int off = MC_HDR_BYTES, on = 0;
    for (int c = 0; c < MC_KWAY; c++) {
        if (off + clen[c] > comp_len) return -1;
        on  += mc_decode_chunk(comp + off, clen[c], rlen[c], out + on);
        off += clen[c];
    }
    return on;
}
