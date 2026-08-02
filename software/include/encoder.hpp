#ifndef ENCODER_HPP
#define ENCODER_HPP

#include "buffer.hpp"
#include "coder_types.hpp"

#include <cstdint>
#include <limits>


class Encoder {
private:
    CodeType _low = std::numeric_limits<uint32_t>::min();
    CodeType _high = std::numeric_limits<uint32_t>::max();
    std::size_t _num_pending_bits = 0;
    Buffer _buffer;

public:
    void encode_symbol(uint32_t cum_low, uint32_t cum_high, uint32_t total);
    [[nodiscard]] auto finish() -> std::vector<uint8_t>;
};

#endif // ENCODER_HPP
