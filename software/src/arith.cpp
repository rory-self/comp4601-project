#include "arith.hpp"
#include "frequency_table.hpp"
#include "encoder.hpp"

auto arith_encode(const Payload& input) -> Payload {
    FrequencyTable table(input);

    Encoder encoder;
    const std::size_t total_bytes = table.get_total();
    for (uint8_t byte : input) {
        const auto [cum_low, cum_high] = table.get_symbol_range(byte);
        encoder.encode_symbol(cum_low, cum_high, total_bytes);
    }

    return encoder.finish();
}

