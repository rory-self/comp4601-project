#include "arith.hpp"
#include <thread>
#include <algorithm>

auto arith_encode_replicated(const Payload& input, const std::size_t num_threads) -> std::vector<Payload> {
    const std::size_t n = std::max<std::size_t>(1, num_threads);
    const std::size_t chunk = (input.size() + n - 1) / n;

    std::vector<Payload> results(n);
    std::vector<std::thread> threads;
    threads.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t begin = std::min(i * chunk, input.size());
        const std::size_t end = std::min(begin + chunk, input.size());
        threads.emplace_back([&results, &input, i, begin, end] {
            results.at(i) = arith_encode(input.data() + begin, end - begin);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    return results;
}

auto arith_decode_replicated(const std::vector<Payload>& chunks, const std::size_t num_threads) -> Payload {
    const std::size_t n = chunks.size();
    std::vector<Payload> decoded(n);
    std::vector<std::thread> threads;
    threads.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        threads.emplace_back([&decoded, &chunks, i] {
            decoded.at(i) = arith_decode(chunks.at(i));
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    Payload output;
    std::size_t total = 0;
    for (const auto& d : decoded) {
        total += d.size();
    }
    output.reserve(total);
    for (const auto& d : decoded) {
        output.insert(output.end(), d.cbegin(), d.cend());
    }

    return output;
}