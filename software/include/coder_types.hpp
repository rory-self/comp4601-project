#ifndef CODER_TYPES_HPP
#define CODER_TYPES_HPP

#include <cstdint>

using CodeType = uint32_t;

constexpr CodeType one_half = 0x80000000U;
constexpr CodeType one_quarter = 0x40000000U;
constexpr CodeType three_quarters = 0xC0000000U;

#endif // CODER_TYPES_HPP
