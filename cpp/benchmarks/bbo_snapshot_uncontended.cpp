#include "llab/bbo_snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <array>
#include <string_view>

template <typename Cell> std::uint64_t run_sample() {
    constexpr std::uint64_t operations = 100'000;
    Cell cell;
    std::uint64_t digest = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t value = 1; value <= operations; ++value) {
        cell.publish({value, value + 1, value + 2, value + 3});
        const auto snapshot = cell.try_read();
        if (!snapshot)
            return 2;
        digest ^= snapshot->bid_price;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (digest == 0xFFFFFFFFFFFFFFFFULL)
        std::cerr << digest;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
}

template <typename Cell> void report(const char *name) {
    constexpr std::size_t warmup = 10;
    constexpr std::size_t samples_count = 200;
    for (std::size_t i = 0; i < warmup; ++i)
        static_cast<void>(run_sample<Cell>());
    std::array<std::uint64_t, samples_count> samples{};
    for (auto &sample : samples)
        sample = run_sample<Cell>();
    std::sort(samples.begin(), samples.end());
    std::cout << name << " operations_per_sample=100000 p50_ns=" << samples[99]
              << " p99_ns=" << samples[197] << '\n';
}

int main(int argc, char **argv) {
    const bool reverse = argc == 2 && std::string_view(argv[1]) == "--reverse";
    if (reverse) {
        report<llab::BboSnapshotCell>("aligned");
        report<llab::UnalignedBboSnapshotCell>("unaligned");
    } else {
        report<llab::UnalignedBboSnapshotCell>("unaligned");
        report<llab::BboSnapshotCell>("aligned");
    }
}
