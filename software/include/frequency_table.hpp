#ifndef FREQUENCY_TABLE_HPP
#define FREQUENCY_TABLE_HPP

#include <cstdint>
#include <array>
#include <vector>

using SymbolRange = std::pair<uint32_t, uint32_t>;

struct LookupResult {
    uint8_t symbol;
    uint32_t cum_low;
    uint32_t cum_high;
};

class FrequencyTable {
private:
    inline static constexpr std::size_t num_unique_bytes = 256;
    inline static constexpr uint32_t max_total_freq = 1u << 16;

    std::array<uint32_t, num_unique_bytes> _freq;
    std::array<uint32_t, num_unique_bytes + 1> _cum_freq;
    std::size_t _total;

    FrequencyTable();

    void rebuild_cumulative_frequencies();
    void normalize();

public:
    [[nodiscard]] static auto build(const std::vector<uint8_t>& input) -> FrequencyTable;
    [[nodiscard]] static auto build(const uint8_t* data, std::size_t len) -> FrequencyTable;
    [[nodiscard]] static auto deserialize(const std::vector<uint8_t>& data, std::size_t& offset) -> FrequencyTable;

    [[nodiscard]] auto get_symbol_range(uint8_t byte) const -> SymbolRange;
    [[nodiscard]] auto get_total() const noexcept -> std::size_t;
    [[nodiscard]] auto serialize() const -> std::vector<uint8_t>;
    [[nodiscard]] auto get_symbol_for_cumulative(uint32_t scaled_value) const -> LookupResult;
};

#endif // FREQUENCY_TABLE_HPP
