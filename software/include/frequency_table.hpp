#ifndef FREQUENCY_TABLE_HPP
#define FREQUENCY_TABLE_HPP

#include <cstdint>
#include <array>
#include <vector>

using SymbolRange = std::pair<uint32_t, uint32_t>;

class FrequencyTable {
private:
    inline static constexpr std::size_t num_unique_bytes = 256;
    inline static constexpr uint32_t max_total_freq = 1u << 16;

    std::array<uint32_t, num_unique_bytes> _freq;
    std::array<uint32_t, num_unique_bytes + 1> _cum_freq;
    std::size_t _total;

    void rebuild_cumulative_frequencies();
    void normalize();

public:
    FrequencyTable(const std::vector<uint8_t>& input);

    [[nodiscard]] auto get_symbol_range(uint8_t byte) const -> SymbolRange;
    [[nodiscard]] auto get_total() const noexcept -> std::size_t;

};

#endif // FREQUENCY_TABLE_HPP
