#include "frequency_table.hpp"

FrequencyTable::FrequencyTable() {
    _total = 0;
    _freq.fill(0);
    _cum_freq.fill(0);
}

void FrequencyTable::rebuild_cumulative_frequencies() {
    uint32_t running_total = 0;

    for (std::size_t i = 0; i < num_unique_bytes; ++i) {
        _cum_freq.at(i) = running_total;
        running_total += _freq.at(i);
    }
    _cum_freq.at(num_unique_bytes) = running_total;
    _total = running_total;
}

void FrequencyTable::normalize() {
    if (_total <= max_total_freq) {
        return;
    }

    const std::size_t old_total = _total;
    std::array<uint32_t, num_unique_bytes> scaled{};

    std::size_t new_total = 0;
    for (std::size_t i = 0; i < num_unique_bytes; ++i) {
        if (_freq.at(i) == 0) {
            scaled.at(i) = 0;
            continue;
        }

        std::size_t v = static_cast<uint64_t>(_freq.at(i)) * max_total_freq / old_total;
        if (v < 1) {
            v = 1;
        }

        const uint32_t scaled_value = static_cast<uint32_t>(v);
        scaled.at(i) = scaled_value;
        new_total += scaled_value;
    }

    while (new_total > max_total_freq) {
        int max_idx = -1;
        for (std::size_t i = 0; i < num_unique_bytes; ++i) {
            if (scaled.at(i) > 1 and (max_idx == -1 or scaled.at(i) > scaled.at(max_idx))) {
                max_idx = i;
            }
        }

        if (max_idx == -1) {
            break;
        }
        scaled.at(max_idx)--;
        new_total--;
    }

    _freq = scaled;
    rebuild_cumulative_frequencies();
}

auto FrequencyTable::build(const std::vector<uint8_t>& input) -> FrequencyTable {
    return build(input.data(), input.size());
}

auto FrequencyTable::build(const uint8_t* data, const std::size_t len) -> FrequencyTable {
    FrequencyTable table;

    for (std::size_t i = 0; i < len; ++i) {
        table._freq.at(data[i])++;
    }

    table.rebuild_cumulative_frequencies();
    table.normalize();

    return table;
}

auto FrequencyTable::get_symbol_range(const uint8_t symbol) const -> SymbolRange {
    return { _cum_freq.at(symbol), _cum_freq.at(symbol + 1) };
}

auto FrequencyTable::get_total() const noexcept -> std::size_t {
    return _total;
}

auto FrequencyTable::serialize() const -> std::vector<uint8_t> {
    std::vector<uint8_t> output;

    for (std::size_t i = 0; i < num_unique_bytes; ++i) {
        uint32_t value = _freq.at(i);

        for (std::size_t j = 0; j < 4; j++) {
            output.push_back(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    return output;
}

auto FrequencyTable::deserialize(const std::vector<uint8_t>& data, std::size_t& offset) -> FrequencyTable {
    FrequencyTable table;

    for (std::size_t i = 0; i < num_unique_bytes; ++i) {
        uint32_t value = static_cast<uint32_t>(data.at(offset));
        for (std::size_t j = 1; j < 4; ++j) {
            const std::size_t local_offset = 8 * j;
            value |= static_cast<uint32_t>(data.at(offset + j)) << local_offset;
        }

        table._freq.at(i) = value;
        offset += 4;
    }
    table.rebuild_cumulative_frequencies();

    return table;
}

auto FrequencyTable::get_symbol_for_cumulative(uint32_t scaled_value) const -> LookupResult {
    std::size_t low = 0, high = num_unique_bytes - 1;
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (_cum_freq.at(mid + 1) <= scaled_value) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return {
        .symbol = static_cast<uint8_t>(low),
        .cum_low = _cum_freq.at(low),
        .cum_high = _cum_freq.at(low + 1)
    };
}