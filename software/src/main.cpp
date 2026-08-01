#include "arith.hpp"

#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace {
using Args = std::unordered_set<std::string_view>;

[[nodiscard]] auto collect_args(int argc, char* argv[]) -> Args;
[[nodiscard]] auto generate_dummy_input() -> Payload;
}

auto main(int argc, char* argv[]) -> int {
    const Args args = collect_args(argc, argv);

    using Clock = std::chrono::high_resolution_clock;

    // TODO read input from file or command line
    const Payload input = generate_dummy_input();

    const auto start_time = Clock::now();
    const std::vector<uint8_t> encoded_result = arith_encode(input);
    const auto end_time = Clock::now();

    if (args.contains("--timing")) {
        const auto total_time = static_cast<std::chrono::nanoseconds>(end_time - start_time);
        std::cout << total_time.count() << " ns\n";
    }

    // TODO temporary
    for (const uint8_t byte : encoded_result) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
    }
    std::cout << '\n';

    return 0;
}

namespace {
[[nodiscard]] auto collect_args(int argc, char* argv[]) -> Args {
    Args args;

    if (argc <= 1) {
        return args;
    }

    args.reserve(argc - 1);

    for (std::size_t i = 1; i < static_cast<std::size_t>(argc); i++) {
        std::string_view arg(argv[i]);
        
        args.insert(arg);
    }

    return args;
}

[[nodiscard]] auto generate_dummy_input() -> Payload {
    constexpr std::size_t num_dummy_bytes = 4096;

    Payload dummy_bytes;
    dummy_bytes.reserve(num_dummy_bytes);

    for (std::size_t i = 0; i < num_dummy_bytes; i++) {
        const uint8_t dummy_byte = static_cast<uint32_t>('a' + (i % 7));
        dummy_bytes.push_back(dummy_byte);
    }

    return dummy_bytes;
}
}
