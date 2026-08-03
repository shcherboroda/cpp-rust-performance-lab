#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

constexpr std::size_t kInputLength = 1'048'576;
constexpr std::uint64_t kSeed = 0x123456789ABCDEF0ULL;

class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

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

[[nodiscard]] std::uint64_t expected_checksum(const std::vector<std::uint64_t>& values) {
    // Sum halves separately to provide an independent checksum path from the
    // timed full-width accumulator. The reconstruction is modulo 2^64.
    std::uint64_t low_halves = 0;
    std::uint64_t high_halves = 0;
    for (const std::uint64_t value : values) {
        low_halves += static_cast<std::uint32_t>(value);
        high_halves += value >> 32U;
    }

    const std::uint64_t low = low_halves & 0xFFFFFFFFULL;
    const std::uint64_t carry = low_halves >> 32U;
    const std::uint64_t high = (high_halves + carry) & 0xFFFFFFFFULL;
    return (high << 32U) | low;
}

[[nodiscard]] std::uint64_t sequential_sum(const std::vector<std::uint64_t>& values) {
    std::uint64_t accumulator = 0;
    for (const std::uint64_t value : values) {
        accumulator += value;
    }
    return accumulator;
}

}  // namespace

int main() {
    std::vector<std::uint64_t> values;
    values.reserve(kInputLength);

    SplitMix64 generator(kSeed);
    for (std::size_t index = 0; index < kInputLength; ++index) {
        values.push_back(generator.next());
    }

    const std::uint64_t expected = expected_checksum(values);
    const std::uint64_t checksum = sequential_sum(values);
    if (checksum != expected) {
        std::cerr << "checksum validation failed: expected 0x" << std::hex << expected
                  << ", got 0x" << checksum << '\n';
        return 1;
    }

    std::cout << "checksum decimal: " << std::dec << checksum << '\n';
    std::cout << "checksum hexadecimal: 0x" << std::hex << std::uppercase << checksum << '\n';
    return 0;
}
