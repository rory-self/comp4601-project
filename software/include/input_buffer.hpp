#ifndef INPUT_BUFFER_HPP
#define INPUT_BUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

class InputBuffer {
private:
    const std::vector<uint8_t>& _data;
    std::size_t _byte_pos;
    std::size_t _bit_pos = 0;

public:
    explicit InputBuffer(const std::vector<uint8_t>& data, std::size_t _start_offset);

    [[nodiscard]] auto get_bit() -> uint8_t;
};

#endif // INPUT_BUFFER_HPP
