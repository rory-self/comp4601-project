#include "encoder.hpp"

void Encoder::encode_symbol(const uint32_t cum_low, const uint32_t cum_high, const uint32_t total) {
    const uint64_t range = static_cast<uint64_t>(_high) - _low + 1;
    _high = _low + static_cast<CodeType>((range * cum_high) / total - 1);
    _low = _low + static_cast<CodeType>((range * cum_low) / total);

    while (true) {
        if (_high < one_half) {
            _buffer.push_bit_and_all_pending(0x00, _num_pending_bits);
        } else if (_low >= one_half) {
            _buffer.push_bit_and_all_pending(0x01, _num_pending_bits);
        } else if (_low >= one_quarter and _high < three_quarters) {
            ++_num_pending_bits;
            _low -= one_quarter;
            _high -= one_quarter;
        } else {
            break;
        }

        _low <<= 1;
        _high = (_high << 1) | 1;
    }
}

auto Encoder::finish() -> std::vector<uint8_t> {
    ++_num_pending_bits;

    const uint8_t bit_to_push = _low < one_quarter ? 0x00 : 0x01;
    _buffer.push_bit_and_all_pending(bit_to_push, _num_pending_bits);

    return _buffer.finish();
}
