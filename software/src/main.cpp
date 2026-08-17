#include "arith.hpp"

#include <unordered_map>
#include <chrono>
#include <iostream>
#include <fstream>
#include <optional>
#include <string>

namespace {
using Args = std::unordered_map<std::string, std::optional<std::string>>;
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr const char *timing_flag = "--timing";
constexpr const char *file_flag = "--file";
constexpr const char *threads_flag = "--threads";

[[nodiscard]] auto collect_args(int argc, char* argv[]) -> Args;
[[nodiscard]] auto generate_dummy_input() -> Payload;
[[nodiscard]] auto read_file_to_bytes(const std::string& file_path) -> Payload;
[[nodiscard]] auto get_input(const Args& args) -> Payload;
[[nodiscard]] auto get_num_threads(const Args& args) -> std::size_t;
void write_bytes_to_file(const std::vector<uint8_t>& data);
void print_timing_data(TimePoint start, TimePoint end);
} // namespace

auto main(int argc, char* argv[]) -> int {
    const Args args = collect_args(argc, argv);
    const bool report_timing = args.find(timing_flag) != args.cend();

    const Payload input = get_input(args);
    const std::size_t num_threads = get_num_threads(args);

    const auto encode_start = Clock::now();
    const std::vector<Payload> chunks = arith_encode_replicated(input, num_threads);
    const auto encode_end = Clock::now();

    std::size_t encoded_size = 0;
    for (const auto& chunk : chunks) {
        encoded_size += chunk.size();
    }

    std::cout << "encode complete\n";
    std::cout << "Threads: " << num_threads << ". Pre-encode size: " << input.size()
              << ". Post-encode size: " << encoded_size << ".\n";

    if (report_timing) {
        print_timing_data(encode_start, encode_end);
    }

    const auto decode_start = Clock::now();
    const Payload decoded = arith_decode_replicated(chunks, num_threads);
    const auto decode_end = Clock::now();

    std::cout << "decode complete\n";

    if (report_timing) {
        print_timing_data(decode_start, decode_end);
    }

    if (decoded != input) {
        std::cerr << "ROUNDTRIP MISMATCH\n";
        write_bytes_to_file(decoded);
        return 1;
    }

    std::cout << "roundtrip ok\n";
    write_bytes_to_file(decoded);

    return 0;
}

namespace {
auto get_input(const Args& args) -> Payload {
    return args.find(file_flag) != args.cend()
        ? read_file_to_bytes(args.at(file_flag).value())
        : generate_dummy_input();
}

auto get_num_threads(const Args& args) -> std::size_t {
    const auto it = args.find(threads_flag);
    if (it == args.cend() or not it->second.has_value()) {
        return 1;
    }

    const int parsed = std::stoi(it->second.value());
    if (parsed < 1) {
        throw std::runtime_error("Thread count must be at least 1");
    }

    return static_cast<std::size_t>(parsed);
}

auto read_file_to_bytes(const std::string& file_path) -> Payload {
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
    const auto total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
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
        std::string arg(argv[i]);

        if (arg == file_flag or arg == threads_flag) {
            ++i;
            if (i >= num_args) {
                throw std::runtime_error("Missing argument after " + arg);
            }

            args[arg] = std::string(argv[i]);
            continue;
        }

        args[arg] = std::nullopt;
    }

    return args;
}

auto generate_dummy_input() -> Payload {
    constexpr std::size_t num_bytes = 4096;
    std::vector<uint8_t> data;
    data.reserve(num_bytes);

    for (std::size_t i = 0; i < num_bytes; i++) {
        const uint8_t byte = static_cast<uint8_t>('a' + (i % 7));
        data.push_back(byte);
    }

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