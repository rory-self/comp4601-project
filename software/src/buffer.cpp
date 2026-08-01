#include "buffer.hpp"

void Buffer::push_bit(const uint8_t bit) {
    _curr_byte = (_curr_byte << 1) | (bit & 1);
    ++_bit_count;

    if (_bit_count == 8) {
        _buffer.push_back(_curr_byte);
        _curr_byte = 0;
        _bit_count = 0;
    }
}

void Buffer::push_bit_and_all_pending(const uint8_t bit, std::size_t& num_pending_bits) {
    push_bit(bit);

    while (num_pending_bits > 0) {
        push_bit(bit ^ 1);
        --num_pending_bits;
    }
}

auto Buffer::finish() -> std::vector<uint8_t> {
    if (_bit_count > 0) {
        _curr_byte = static_cast<uint8_t>(_curr_byte << (8 - _bit_count));
        _buffer.push_back(_curr_byte);
        _curr_byte = 0;
        _bit_count = 0;
    }

    return std::move(_buffer);
}
