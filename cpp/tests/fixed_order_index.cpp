#include "llab/fixed_order_index.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::uint64_t mix(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

std::array<std::uint64_t, 4> colliding_ids() {
    std::array<std::vector<std::uint64_t>, 8> buckets;
    for (std::uint64_t id = 1; id < 10'000; ++id) {
        auto &bucket = buckets[mix(id) & 7U];
        bucket.push_back(id);
        if (bucket.size() == 4)
            return {bucket[0], bucket[1], bucket[2], bucket[3]};
    }
    throw std::runtime_error("failed to find collision cluster");
}

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

void collision_cluster_survives_deletion() {
    llab::FixedOrderIndex<std::uint64_t> index(4);
    const auto ids = colliding_ids();
    for (std::size_t i = 0; i < ids.size(); ++i)
        require(index.insert(ids[i], 100 + i), "insert failed");

    require(index.erase(ids[1]) == std::optional<std::uint64_t>{101}, "middle erase failed");
    require(index.find(ids[0]) && *index.find(ids[0]) == 100, "head was lost");
    require(index.find(ids[2]) && *index.find(ids[2]) == 102, "survivor was lost");
    require(index.find(ids[3]) && *index.find(ids[3]) == 103, "tail was lost");
    require(!index.find(ids[1]), "deleted entry remains visible");

    require(index.erase(ids[0]).has_value(), "head erase failed");
    require(index.erase(ids[3]).has_value(), "tail erase failed");
    require(index.find(ids[2]) && *index.find(ids[2]) == 102, "tail survivor was lost");
}

void long_history_preserves_capacity() {
    llab::FixedOrderIndex<std::uint64_t> index(32);
    for (std::uint64_t generation = 0; generation < 2'000; ++generation) {
        const std::uint64_t id = generation + 1;
        require(index.insert(id, generation), "history insert failed");
        require(index.erase(id) == std::optional<std::uint64_t>{generation},
                "history erase failed");
    }
    require(index.size() == 0, "history did not return to empty");
    for (std::uint64_t id = 1; id <= 32; ++id)
        require(index.insert(id, id * 10), "post-history insert failed");
    for (std::uint64_t id = 1; id <= 32; ++id)
        require(index.find(id) && *index.find(id) == id * 10, "post-history lookup failed");
}

void randomized_history_matches_reference_model() {
    llab::FixedOrderIndex<std::uint64_t> index(64);
    std::unordered_map<std::uint64_t, std::uint64_t> reference;
    std::uint64_t random = 0xD1B54A32D192ED03ULL;
    for (std::size_t step = 0; step < 50'000; ++step) {
        random = random * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint64_t id = 1 + ((random >> 16U) % 128U);
        const std::uint64_t value = random ^ (random >> 29U);
        switch (random & 3U) {
        case 0:
            if (reference.size() < 64 && !reference.contains(id)) {
                require(index.insert(id, value), "model insert failed");
                reference.emplace(id, value);
            }
            break;
        case 1: {
            const auto expected = reference.find(id);
            const auto erased = index.erase(id);
            require(erased.has_value() == (expected != reference.end()), "model erase presence");
            if (expected != reference.end()) {
                require(*erased == expected->second, "model erase value");
                reference.erase(expected);
            }
            break;
        }
        default: {
            const auto expected = reference.find(id);
            const auto *found = index.find(id);
            require((found != nullptr) == (expected != reference.end()), "model lookup presence");
            if (found)
                require(*found == expected->second, "model lookup value");
            break;
        }
        }
        require(index.size() == reference.size(), "model size mismatch");
    }
}

} // namespace

int main() {
    collision_cluster_survives_deletion();
    long_history_preserves_capacity();
    randomized_history_matches_reference_model();

    llab::FixedOrderIndex<std::uint32_t> iteration(4);
    require(iteration.insert(1, 11), "iteration insert one");
    require(iteration.insert(2, 22), "iteration insert two");
    std::uint32_t sum = 0;
    iteration.for_each([&](auto, auto value) { sum += value; });
    require(sum == 33, "iteration sum");
    const auto probes = iteration.probe_summary();
    require(probes.occupied == 2 && probes.total_probes >= 2 && probes.maximum_probes >= 1,
            "probe summary");
}
