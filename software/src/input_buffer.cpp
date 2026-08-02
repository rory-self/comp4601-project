#include "input_buffer.hpp"

InputBuffer::InputBuffer(const std::vector<uint8_t>& data, const std::size_t offset)
    : _data(data), _byte_pos(offset) {}

auto InputBuffer::get_bit() -> uint8_t {
    if (_byte_pos >= _data.size()) {
        return 0;
    }

    const uint8_t bit = (_data.at(_byte_pos) >> (7 - _bit_pos)) & 1;
    
    ++_bit_pos;
    if (_bit_pos == 8) {
        _bit_pos = 0;
        ++_byte_pos;
    }

    return bit;
}
