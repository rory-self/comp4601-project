#include "arith.hpp"
#include "frequency_table.hpp"
#include "encoder.hpp"
#include "decoder.hpp"

#include <stdexcept>

namespace {
void write_length_to_header(std::vector<uint8_t>& out, uint64_t v);
[[nodiscard]] auto read_length_from_header(const Payload& input, std::size_t& offset) -> std::size_t;
} // namespace

auto arith_encode(const Payload& input) -> Payload {
    return arith_encode(input.data(), input.size());
}

auto arith_encode(const uint8_t* data, const std::size_t len) -> Payload {
    FrequencyTable table = FrequencyTable::build(data, len);

    std::vector<uint8_t> output;
    write_length_to_header(output, static_cast<uint64_t>(len));
    const std::vector<uint8_t> table_serial = table.serialize();
    output.insert(output.end(), table_serial.cbegin(), table_serial.cend());

    Encoder encoder;
    const uint32_t total_bytes = static_cast<uint32_t>(table.get_total());
    for (std::size_t i = 0; i < len; ++i) {
        const auto [cum_low, cum_high] = table.get_symbol_range(data[i]);
        encoder.encode_symbol(cum_low, cum_high, total_bytes);
    }

    const std::vector<uint8_t> encoded_data = encoder.finish();
    output.insert(output.end(), encoded_data.cbegin(), encoded_data.cend());

    return output;
}

auto arith_decode(const Payload &input) -> Payload {
    std::size_t offset = 0;
    const std::size_t original_length = read_length_from_header(input, offset);

    const FrequencyTable table = FrequencyTable::deserialize(input, offset);
    const uint32_t total = table.get_total();

    std::vector<uint8_t> output;
    output.reserve(original_length);

    Decoder decoder(input, offset);
    for (std::size_t i = 0; i < original_length; ++i) {
        const uint32_t scaled_value = decoder.get_current_count(total);

        const auto [byte, cum_low, cum_high] = table.get_symbol_for_cumulative(scaled_value);
        decoder.update(cum_low, cum_high, total);
        output.push_back(byte);
    }

    return output;
}

namespace {
void write_length_to_header(std::vector<uint8_t>& out, const uint64_t v) {
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

[[nodiscard]] auto read_length_from_header(const Payload& input, std::size_t& offset) -> std::size_t {
    if (offset + 8 > input.size()) {
        throw std::runtime_error("truncated header");
    }

    uint64_t length = 0;
    for (int i = 0; i < 8; i++) {
        length |= static_cast<uint64_t>(input.at(offset + i)) << (8 * i);
    }

    offset += 8;
    return length;
}
} // namespace