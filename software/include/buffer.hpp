#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <cstdint>
#include <vector>

class Buffer {
private:
    std::vector<uint8_t> _buffer;
    uint8_t _curr_byte = 0;
    std::size_t _bit_count = 0;

    void push_bit(uint8_t bit);

public:
    void push_bit_and_all_pending(uint8_t bit, std::size_t& num_pending_bits);
    [[nodiscard]] auto finish() -> std::vector<uint8_t>;
};

#endif // BUFFER_HPP
