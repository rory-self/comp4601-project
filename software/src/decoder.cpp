#include "decoder.hpp"
#include "coder_types.hpp"

Decoder::Decoder(const std::vector<uint8_t>& data, const std::size_t offset)
    : _buffer(data, offset) {
    for (std::size_t i = 0; i < sizeof(CodeType) * 8; ++i) {
        _value = (_value << 1) | static_cast<CodeType>(_buffer.get_bit());
    }
}

auto Decoder::get_current_range() const -> uint64_t {
    return static_cast<uint64_t>(_high) - _low + 1;
}

void Decoder::update(uint32_t cum_low, uint32_t cum_high, uint32_t total) {
    const uint64_t range = get_current_range();
    _high = _low + static_cast<CodeType>((range * cum_high) / total - 1);
    _low = _low + static_cast<CodeType>((range * cum_low) / total);

    while (true) {
        if (_high < one_half) {
            // no op
        } else if (_low >= one_half) {
            _low -= one_half;
            _high -= one_half;
            _value -= one_half;
        } else if (_low >= one_quarter and _high < three_quarters) {
            _low -= one_quarter;
            _high -= one_quarter;
            _value -= one_quarter;
        } else {
            break;
        }

        _low <<= 1;
        _high = (_high << 1) | 1;
        _value = (_value << 1) | static_cast<CodeType>(_buffer.get_bit());
    }
}

auto Decoder::get_current_count(const uint32_t total) const -> uint64_t {
    const uint64_t range = get_current_range();
    const uint64_t value64 = static_cast<uint64_t>(_value);
    return static_cast<uint32_t>((((value64 - _low + 1) * total) - 1) / range);
}
