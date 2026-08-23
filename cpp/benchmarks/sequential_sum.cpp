#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

constexpr std::size_t kInputLength = 1'048'576;
constexpr std::size_t kWarmupRuns = 5;
constexpr std::size_t kMeasuredRuns = 100;
constexpr std::uint64_t kSeed = 0x123456789ABCDEF0ULL;
constexpr std::uint64_t kExpectedChecksum = 0x839CD625000CDB7AULL;
constexpr std::size_t kP50Index = 49;
constexpr std::size_t kP90Index = 89;
constexpr std::size_t kP95Index = 94;

class SplitMix64 {
  public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {
    }

    [[nodiscard]] std::uint64_t next() {
        // SplitMix64 transition: state += 0x9E3779B97F4A7C15 (mod 2^64);
        // z = state; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
        // z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31).
        state_ += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

  private:
    std::uint64_t state_;
};

[[nodiscard]] std::uint64_t sequential_sum(const std::vector<std::uint64_t> &values) {
    std::uint64_t accumulator = 0;
    for (const std::uint64_t value : values) {
        accumulator += value;
    }
    return accumulator;
}

} // namespace

int main() {
    std::vector<std::uint64_t> values;
    values.reserve(kInputLength);

    SplitMix64 generator(kSeed);
    for (std::size_t index = 0; index < kInputLength; ++index) {
        values.push_back(generator.next());
    }

    for (std::size_t run = 0; run < kWarmupRuns; ++run) {
        const std::uint64_t checksum = sequential_sum(values);
        if (checksum != kExpectedChecksum) {
            std::cerr << "checksum validation failed: expected 0x" << std::hex << kExpectedChecksum
                      << ", got 0x" << checksum << '\n';
            return 1;
        }
    }

    std::vector<std::uint64_t> durations_ns;
    durations_ns.reserve(kMeasuredRuns);
    std::uint64_t checksum = 0;
    for (std::size_t run = 0; run < kMeasuredRuns; ++run) {
        const auto start = std::chrono::steady_clock::now();
        checksum = sequential_sum(values);
        const auto end = std::chrono::steady_clock::now();

        if (checksum != kExpectedChecksum) {
            std::cerr << "checksum validation failed: expected 0x" << std::hex << kExpectedChecksum
                      << ", got 0x" << checksum << '\n';
            return 1;
        }
        durations_ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    std::vector<std::uint64_t> sorted_durations = durations_ns;
    std::sort(sorted_durations.begin(), sorted_durations.end());
    long double total_ns = 0;
    for (const std::uint64_t duration_ns : durations_ns) {
        total_ns += duration_ns;
    }
    const long double mean_ns = total_ns / kMeasuredRuns;
    constexpr long double kInputBytes = kInputLength * sizeof(std::uint64_t);
    constexpr long double kGiB = 1ULL << 30U;
    const long double p50_gib_per_s =
        kInputBytes * 1'000'000'000.0L / (kGiB * sorted_durations[kP50Index]);
    const long double mean_gib_per_s = kInputBytes * 1'000'000'000.0L / (kGiB * mean_ns);

    std::cout << "warmup_runs: " << kWarmupRuns << '\n';
    std::cout << "sample_count: " << kMeasuredRuns << '\n';
    std::cout << "min_ns: " << sorted_durations.front() << '\n';
    std::cout << "p50_ns: " << sorted_durations[kP50Index] << '\n';
    std::cout << "p90_ns: " << sorted_durations[kP90Index] << '\n';
    std::cout << "p95_ns: " << sorted_durations[kP95Index] << '\n';
    std::cout << "max_ns: " << sorted_durations.back() << '\n';
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "mean_ns: " << mean_ns << '\n';
    std::cout << "p50_gib_per_s: " << p50_gib_per_s << '\n';
    std::cout << "mean_gib_per_s: " << mean_gib_per_s << '\n';
    std::cout << "checksum decimal: " << std::dec << checksum << '\n';
    std::cout << "checksum hexadecimal: 0x" << std::hex << std::uppercase << checksum << '\n';
    return 0;
}
