#include "frequency_table.hpp"

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

        std::size_t v = static_cast<uint64_t>((_freq.at(i)) * max_total_freq) / old_total;
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

FrequencyTable::FrequencyTable(const std::vector<uint8_t>& input) {
    _total = 0;
    _freq.fill(0);
    _cum_freq.fill(0);

    for (uint8_t byte : input) {
        _freq.at(byte)++;
    }

    rebuild_cumulative_frequencies();
    normalize();
}

auto FrequencyTable::get_symbol_range(const uint8_t symbol) const -> SymbolRange {
    return { _cum_freq.at(symbol), _cum_freq.at(symbol + 1) };
}

auto FrequencyTable::get_total() const noexcept -> std::size_t {
    return _total;
}

