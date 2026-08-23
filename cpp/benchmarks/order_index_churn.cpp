#include "llab/fixed_order_index.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t kLiveOrders = 16'384;
constexpr std::size_t kChurnOperations = 32'768;
constexpr std::size_t kWarmups = 10;
constexpr std::size_t kSamples = 200;
std::atomic<std::uint64_t> g_sink = 0;

struct TombstoneIndex {
    struct Slot {
        std::uint64_t id = 0;
        std::uint32_t value = 0;
        enum class State : std::uint8_t { Empty, Occupied, Tombstone } state = State::Empty;
    };

    explicit TombstoneIndex(std::size_t maximum_live) : slots(capacity(maximum_live)) {
    }

    bool insert(std::uint64_t id, std::uint32_t value) {
        const auto mask = slots.size() - 1;
        std::size_t first_tombstone = slots.size();
        for (std::size_t i = mix(id) & mask, probes = 0; probes < slots.size();
             ++probes, i = (i + 1) & mask) {
            auto &slot = slots[i];
            if (slot.state == Slot::State::Empty) {
                slots[first_tombstone == slots.size() ? i : first_tombstone] = {
                    id, value, Slot::State::Occupied};
                return true;
            }
            if (slot.state == Slot::State::Tombstone && first_tombstone == slots.size())
                first_tombstone = i;
            if (slot.state == Slot::State::Occupied && slot.id == id)
                return false;
        }
        if (first_tombstone != slots.size()) {
            slots[first_tombstone] = {id, value, Slot::State::Occupied};
            return true;
        }
        return false;
    }

    bool erase(std::uint64_t id) {
        const auto mask = slots.size() - 1;
        for (std::size_t i = mix(id) & mask, probes = 0; probes < slots.size();
             ++probes, i = (i + 1) & mask) {
            auto &slot = slots[i];
            if (slot.state == Slot::State::Empty)
                return false;
            if (slot.state == Slot::State::Occupied && slot.id == id) {
                slot.state = Slot::State::Tombstone;
                return true;
            }
        }
        return false;
    }

    std::vector<Slot> slots;

    static std::uint64_t mix(std::uint64_t value) {
#if defined(LLAB_IDENTITY_HASH)
        return value;
#elif defined(LLAB_MULTIPLICATIVE_HASH)
        return value * 0x9E3779B97F4A7C15ULL;
#else
        return llab::detail::mix_fixed_order_id(value);
#endif
    }

    static std::size_t capacity(std::size_t maximum_live) {
        return llab::detail::fixed_order_index_capacity(maximum_live);
    }
};

template <typename Index> std::uint64_t run_sample() {
    Index index(kLiveOrders);
    std::vector<std::uint64_t> live(kLiveOrders);
    for (std::size_t i = 0; i < live.size(); ++i) {
        live[i] = i + 1;
        if (!index.insert(live[i], static_cast<std::uint32_t>(i)))
            throw std::logic_error("initial insert failed");
    }

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t digest = 0;
    for (std::size_t operation = 0; operation < kChurnOperations; ++operation) {
        const std::size_t victim = (operation * 40503U) % kLiveOrders;
        if (!index.erase(live[victim]))
            throw std::logic_error("erase failed");
        const std::uint64_t replacement = 1'000'000 + operation;
        if (!index.insert(replacement, static_cast<std::uint32_t>(operation)))
            throw std::logic_error("replacement insert failed");
        live[victim] = replacement;
        digest ^= replacement;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    g_sink.fetch_xor(digest, std::memory_order_relaxed);
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

template <typename Index> std::vector<std::uint64_t> samples() {
    for (std::size_t i = 0; i < kWarmups; ++i)
        static_cast<void>(run_sample<Index>());
    std::vector<std::uint64_t> result;
    result.reserve(kSamples);
    for (std::size_t i = 0; i < kSamples; ++i)
        result.push_back(run_sample<Index>());
    return result;
}

template <typename Index> int main_for(std::string_view raw) {
    const auto values = samples<Index>();
    if (!raw.empty()) {
        std::ofstream output{std::string(raw)};
        for (auto value : values)
            output << value << '\n';
    }
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    std::cout << "samples=" << values.size() << " p50_ns=" << sorted[sorted.size() / 2]
              << " p99_ns=" << sorted[sorted.size() * 99 / 100] << '\n';
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    std::string_view raw;
    if (argc == 3 && std::string_view(argv[1]) == "--raw")
        raw = argv[2];
    else if (argc != 1)
        throw std::invalid_argument("usage: order_index_churn [--raw path]");
#ifdef LLAB_BACKSHIFT_INDEX
    return main_for<llab::FixedOrderIndex<std::uint32_t>>(raw);
#else
    return main_for<TombstoneIndex>(raw);
#endif
}
