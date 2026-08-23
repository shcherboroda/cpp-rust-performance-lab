#include "llab/feed_sequence_recovery.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t kEvents = 262'144;
constexpr std::size_t kWarmups = 10;
constexpr std::size_t kSamples = 200;

std::uint64_t run_sample() {
    llab::FeedSequenceRecovery<std::uint64_t> recovery(0);
    std::vector<std::uint64_t> ready;
    ready.reserve(kEvents);
    recovery.begin_snapshot(0);
    if (!recovery.finish_snapshot(ready))
        throw std::logic_error("initial snapshot");

    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t sequence = 1; sequence <= kEvents; ++sequence)
        if (recovery.on_incremental(sequence, sequence, ready) !=
            llab::FeedIncrementalResult::Apply)
            throw std::logic_error("incremental");
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (ready.size() != kEvents)
        throw std::logic_error("ready count");
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

} // namespace

int main(int argc, char **argv) {
    std::string_view raw;
    if (argc == 3 && std::string_view(argv[1]) == "--raw")
        raw = argv[2];
    else if (argc != 1)
        throw std::invalid_argument("usage: feed_sequence_recovery [--raw path]");
    for (std::size_t i = 0; i < kWarmups; ++i)
        static_cast<void>(run_sample());
    std::vector<std::uint64_t> values;
    values.reserve(kSamples);
    for (std::size_t i = 0; i < kSamples; ++i)
        values.push_back(run_sample());
    if (!raw.empty()) {
        std::ofstream output{std::string(raw)};
        for (const auto value : values)
            output << value << '\n';
    }
    std::sort(values.begin(), values.end());
    std::cout << "samples=" << values.size() << " p50_ns=" << values[values.size() / 2]
              << " p99_ns=" << values[values.size() * 99 / 100] << '\n';
}
