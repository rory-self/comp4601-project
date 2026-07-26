#ifndef ARITH_HPP
#define ARITH_HPP

#include <cstddef>
#include <vector>

using Payload = std::vector<std::byte>;

[[nodiscard]] auto arith_encode(const Payload& input) -> Payload;

#endif // ARITH_HPP
