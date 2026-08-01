#ifndef ARITH_HPP
#define ARITH_HPP

#include <cstdint>
#include <vector>

using Payload = std::vector<uint8_t>;

[[nodiscard]] auto arith_encode(const Payload& input) -> Payload;
[[nodiscard]] auto arith_decode(const Payload& input) -> Payload;

#endif // ARITH_HPP
