#include "arith.hpp"

#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace {
using Args = std::unordered_set<std::string_view>;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::system_clock::time_point;

constexpr const char *timing_flag = "--timing";

[[nodiscard]] auto collect_args(int argc, char* argv[]) -> Args;
[[nodiscard]] auto generate_dummy_input() -> Payload;
auto run_encode(const Args& args) -> Payload;
void run_decode(const Args& args, const Payload& payload);
void print_timing_data(TimePoint start, TimePoint end);
} // namespace

auto main(int argc, char* argv[]) -> int {
    const Args args = collect_args(argc, argv);

    if (args.contains("--complete")) {
        const Payload encoded_data = run_encode(args);
        run_decode(args, encoded_data);

    } else if (args.contains("--encode")) {
        run_encode(args);

    } else if (args.contains("--decode")) {
        // TODO get data source

    } else {
        std::cout << "No valid coding instruction given.\n";
    }

    return 0;
}

namespace {
auto run_encode(const Args& args) -> Payload {
    // TODO read input from file or command line
    const Payload input = generate_dummy_input();

    const auto start_time = Clock::now();
    const std::vector<uint8_t> encoded_result = arith_encode(input);
    const auto end_time = Clock::now();

    std::cout << "encode complete\n";

    if (args.contains(timing_flag)) {
        print_timing_data(start_time, end_time);
    }

    // TODO temporary
    for (const uint8_t byte : encoded_result) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
    }
    std::cout << '\n';

    return encoded_result;
}

void run_decode(const Args& args, const Payload& payload) {
    const auto start_time = Clock::now();
    const Payload decoded_data = arith_decode(payload);
    const auto end_time = Clock::now();

    std::cout << "decode complete\n";

    if (args.contains(timing_flag)) {
        print_timing_data(start_time, end_time);
    }

    for (const uint8_t byte : decoded_data) {
        std::cout << static_cast<char>(byte);
    }
}

void print_timing_data(const TimePoint start, const TimePoint end) {
    const auto total_time = static_cast<std::chrono::nanoseconds>(end - start);
    std::cout << total_time.count() << " ns\n";
}

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
    const std::string text = "this is a test, I hope it works";
    
    std::vector<uint8_t> data;
    data.insert(data.end(), text.cbegin(), text.cend());

    return data;
}
} // namespace
