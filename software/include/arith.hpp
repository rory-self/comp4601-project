#ifndef ARITH_HPP
#define ARITH_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

using Payload = std::vector<uint8_t>;

[[nodiscard]] auto arith_encode(const Payload& input) -> Payload;
[[nodiscard]] auto arith_encode(const uint8_t* data, std::size_t len) -> Payload;
[[nodiscard]] auto arith_decode(const Payload& input) -> Payload;
[[nodiscard]] auto arith_encode_replicated(const Payload& input, std::size_t num_threads) -> std::vector<Payload>;
[[nodiscard]] auto arith_decode_replicated(const std::vector<Payload>& chunks, std::size_t num_threads) -> Payload;

#endif // ARITH_HPP
