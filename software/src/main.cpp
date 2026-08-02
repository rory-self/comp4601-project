#include "arith.hpp"

#include <unordered_map>
#include <chrono>
#include <iostream>
#include <fstream>

namespace {
using Args = std::unordered_map<std::string_view, std::optional<std::string_view>>;
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::system_clock::time_point;

constexpr const char *timing_flag = "--timing";
constexpr const char *file_flag = "--file";

[[nodiscard]] auto collect_args(int argc, char* argv[]) -> Args;
[[nodiscard]] auto generate_dummy_input() -> Payload;
[[nodiscard]] auto read_file_to_bytes(const std::string_view file_path) -> Payload;
void write_bytes_to_file(const std::vector<uint8_t>& data);
auto run_encode(const Args& args) -> Payload;
void run_decode(const Args& args, const Payload& payload);
void print_timing_data(TimePoint start, TimePoint end);
} // namespace

auto main(int argc, char* argv[]) -> int {
    const Args args = collect_args(argc, argv);

    const Payload encoded_data = run_encode(args);
    run_decode(args, encoded_data);


    return 0;
}

namespace {
auto run_encode(const Args& args) -> Payload {
    // TODO read input from file or command line
    const Payload input = args.contains(file_flag) ? read_file_to_bytes(args.at(file_flag).value())
            : generate_dummy_input();

    const std::size_t pre_encode_size = input.size();

    const auto start_time = Clock::now();
    const std::vector<uint8_t> encoded_result = arith_encode(input);
    const auto end_time = Clock::now();

    std::cout << "encode complete\n";

    std::cout << "Pre-encode size: " << pre_encode_size << ". Post-encode size: " << encoded_result.size() << ".\n";

    if (args.contains(timing_flag)) {
        print_timing_data(start_time, end_time);
    }

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

    write_bytes_to_file(decoded_data);
}

auto read_file_to_bytes(const std::string_view file_path) -> Payload {
    std::string path(file_path);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (not file) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to determine file size: " + path);
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<std::size_t>(size));
    if (size > 0 and not file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + path);
    }

    return buffer;
}

void print_timing_data(const TimePoint start, const TimePoint end) {
    const auto total_time = static_cast<std::chrono::nanoseconds>(end - start);
    std::cout << total_time.count() << " ns\n";
}

auto collect_args(int argc, char* argv[]) -> Args {
    Args args;

    if (argc <= 1) {
        return args;
    }

    args.reserve(argc - 1);

    const std::size_t num_args = static_cast<std::size_t>(argc);
    for (std::size_t i = 1; i < num_args; i++) {
        std::string_view arg(argv[i]);

        if (arg == file_flag) {
            ++i;
            if (i >= num_args) {
                throw std::runtime_error("Missing argument where filepath expected!");
            }

            std::string_view filepath(argv[i]);
            args[arg] = filepath;
            continue;
        }
       
        args[arg] = std::nullopt;
    }

    return args;
}

auto generate_dummy_input() -> Payload {
    const std::string text = "this is a test, I hope it works";
    
    std::vector<uint8_t> data;
    data.insert(data.end(), text.cbegin(), text.cend());

    return data;
}

void write_bytes_to_file(const std::vector<uint8_t>& data) {
    const std::string path = "./output";
    std::ofstream file(path, std::ios::binary);
    if (not file) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    if (not data.empty() and not file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("Failed to write file: " + path);
    }
}
} // namespace
