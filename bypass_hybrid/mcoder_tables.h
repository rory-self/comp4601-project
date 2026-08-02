#ifndef MCODER_TABLES_H_
#define MCODER_TABLES_H_
#include <stdint.h>

/*
 * H.264/AVC CABAC "M-coder" probability tables.
 *   mc_rangeTabLPS : ITU-T H.264 Table 9-44  (64 states x 4 range quantiles)
 *   mc_transIdxLPS : ITU-T H.264 Table 9-45, LPS column
 *   mc_transIdxMPS : ITU-T H.264 Table 9-45, MPS column
 *
 * These three tables are the whole point of the rewrite: they replace the
 * 17x12 multiply (range * prob >> 12) with one ROM read, and replace the
 * adaptive add/shift probability update with one ROM read.
 *
 * Total: 256 + 64 + 64 = 384 bytes.  Fits in a single 18Kb BRAM, or in LUTROM
 * if the tool prefers.
 *
 * Sanity model: p_LPS(sigma) = 0.5 * alpha^sigma, alpha = (0.01875/0.5)^(1/63),
 * so p_LPS(0) = 0.5 and p_LPS(62) = 0.01875.  State 63 is the reserved
 * terminate state (rLPS = 2) and is unreachable by adaptation, because
 * mc_transIdxMPS[62] == 62.
 */

/*
 * Table 9-44, packed four-to-a-word: byte q of mc_rlps4[st] is
 * mc_rangeTabLPS[st][q].  Same 256 bytes, addressed differently, and this is
 * the form the hardware actually reads.
 *
 * Why: the natural lookup rangeTabLPS[st][q] puts the ROM *inside* the
 * loop-carried range dependency, because q = (range >> 6) & 3.  Synthesis
 * measured that path -- range -> subtract -> ROM address -> ROM read -> barrel
 * shift -- at 7.66 ns against a 5 ns budget.  Indexing by st alone lifts the
 * ROM out of that loop: st comes from the context, which is selected by the
 * input bits and is known early, so the read overlaps the range arithmetic and
 * only a 4:1 byte mux is left on the critical path.
 *
 * Generated from mc_rangeTabLPS below; mcoder_test asserts the two agree, so
 * they cannot drift.
 */
static const uint32_t mc_rlps4[64] = {
    0xF0D0B080u, 0xE3C5A780u, 0xD8BB9E80u, 0xCDB2967Bu,
    0xC3A98E74u, 0xB9A0876Fu, 0xAF988069u, 0xA6907A64u,
    0x9E89745Fu, 0x96826E5Au, 0x8E7B6855u, 0x87756351u,
    0x806F5E4Du, 0x7A695949u, 0x74645545u, 0x6E5F5042u,
    0x685A4C3Eu, 0x6356483Bu, 0x5E514538u, 0x594D4135u,
    0x55493E33u, 0x50453B30u, 0x4C42382Eu, 0x483F352Bu,
    0x453B3229u, 0x41383027u, 0x3E362D25u, 0x3B332B23u,
    0x38302921u, 0x352E2720u, 0x322B251Eu, 0x3029231Du,
    0x2D27211Bu, 0x2B251F1Au, 0x29231E18u, 0x27211C17u,
    0x25201B16u, 0x231E1A15u, 0x211D1814u, 0x1F1B1713u,
    0x1E1A1612u, 0x1C191511u, 0x1B171410u, 0x1916130Fu,
    0x1815120Eu, 0x1714110Eu, 0x1613100Du, 0x15120F0Cu,
    0x14110E0Cu, 0x13100E0Bu, 0x120F0D0Bu, 0x110F0C0Au,
    0x100E0C0Au, 0x0F0D0B09u, 0x0E0C0B09u, 0x0E0C0A08u,
    0x0D0B0908u, 0x0C0B0907u, 0x0C0A0907u, 0x0B0A0807u,
    0x0B090806u, 0x0A090706u, 0x09080706u, 0x02020202u,
};

/*
 * Extract rLPS for range quantile q from the packed word.
 *
 * Written as an explicit 4:1 mux over four *constant* shifts, not as
 * (w >> (q << 3)).  The variable-shift form is a 32-bit barrel shifter and
 * synthesis measured it at 5.22 ns on the critical path; constant shifts are
 * free wiring, leaving only an 8-bit 4:1 mux.
 */
#define MC_RLPS(w, q)                                        \
    (((q) & 2u) ? (((q) & 1u) ? (((w) >> 24) & 0xFFu)        \
                              : (((w) >> 16) & 0xFFu))       \
                : (((q) & 1u) ? (((w) >>  8) & 0xFFu)        \
                              : ( (w)        & 0xFFu)))

/* Reference form, indexed [pStateIdx][qRangeIdx].  Kept as the readable
 * source of truth and as the decoder's table; mc_rlps4 is derived from it. */
static const uint8_t mc_rangeTabLPS[64][4] = {
    {128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216},
    {123, 150, 178, 205}, {116, 142, 169, 195}, {111, 135, 160, 185},
    {105, 128, 152, 175}, {100, 122, 144, 166}, { 95, 116, 137, 158},
    { 90, 110, 130, 150}, { 85, 104, 123, 142}, { 81,  99, 117, 135},
    { 77,  94, 111, 128}, { 73,  89, 105, 122}, { 69,  85, 100, 116},
    { 66,  80,  95, 110}, { 62,  76,  90, 104}, { 59,  72,  86,  99},
    { 56,  69,  81,  94}, { 53,  65,  77,  89}, { 51,  62,  73,  85},
    { 48,  59,  69,  80}, { 46,  56,  66,  76}, { 43,  53,  63,  72},
    { 41,  50,  59,  69}, { 39,  48,  56,  65}, { 37,  45,  54,  62},
    { 35,  43,  51,  59}, { 33,  41,  48,  56}, { 32,  39,  46,  53},
    { 30,  37,  43,  50}, { 29,  35,  41,  48}, { 27,  33,  39,  45},
    { 26,  31,  37,  43}, { 24,  30,  35,  41}, { 23,  28,  33,  39},
    { 22,  27,  32,  37}, { 21,  26,  30,  35}, { 20,  24,  29,  33},
    { 19,  23,  27,  31}, { 18,  22,  26,  30}, { 17,  21,  25,  28},
    { 16,  20,  23,  27}, { 15,  19,  22,  25}, { 14,  18,  21,  24},
    { 14,  17,  20,  23}, { 13,  16,  19,  22}, { 12,  15,  18,  21},
    { 12,  14,  17,  20}, { 11,  14,  16,  19}, { 11,  13,  15,  18},
    { 10,  12,  15,  17}, { 10,  12,  14,  16}, {  9,  11,  13,  15},
    {  9,  11,  12,  14}, {  8,  10,  12,  14}, {  8,   9,  11,  13},
    {  7,   9,  11,  12}, {  7,   9,  10,  12}, {  7,   8,  10,  11},
    {  6,   8,   9,  11}, {  6,   7,   9,  10}, {  6,   7,   8,   9},
    {  2,   2,   2,   2}
};

static const uint8_t mc_transIdxLPS[64] = {
     0,  0,  1,  2,  2,  4,  4,  5,  6,  7,  8,  9,  9, 11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63
};

static const uint8_t mc_transIdxMPS[64] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63
};

#endif /* MCODER_TABLES_H_ */
