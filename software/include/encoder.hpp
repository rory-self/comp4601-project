#ifndef ENCODER_HPP
#define ENCODER_HPP

#include "buffer.hpp"
#include "coder_types.hpp"

#include <cstdint>


class Encoder {
private:
    CodeType _low = 0;
    CodeType _high = UINT32_MAX;
    std::size_t _num_pending_bits = 0;
    Buffer _buffer;

public:
    void encode_symbol(uint32_t cum_low, uint32_t cum_high, uint32_t total);
    [[nodiscard]] auto finish() -> std::vector<uint8_t>;
};

#endif // ENCODER_HPP
