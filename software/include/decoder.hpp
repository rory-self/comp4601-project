#ifndef DECODER_HPP
#define DECODER_HPP

#include "coder_types.hpp"
#include "input_buffer.hpp"

class Decoder {
private:
    CodeType _low = std::numeric_limits<CodeType>::min();
    CodeType _high = std::numeric_limits<CodeType>::max();
    CodeType _value = 0;
    InputBuffer _buffer;

    [[nodiscard]] auto get_current_range() const -> uint64_t;

public:
    Decoder(const std::vector<uint8_t>& input, std::size_t offset);

    void update(uint32_t cum_low, uint32_t cum_high, uint32_t total);
    [[nodiscard]] auto get_current_count(uint32_t total) const -> uint64_t;
};

#endif // DECODER_HPP
