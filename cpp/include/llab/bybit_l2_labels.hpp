#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace llab::bybit_l2_labels {

inline std::vector<std::size_t> first_future_indices(const std::span<const std::uint64_t> timestamps,
                                                      const std::uint64_t horizon_ms) {
    std::vector<std::size_t> result(timestamps.size(), timestamps.size());
    std::size_t future = 0;
    for (std::size_t current = 0; current < timestamps.size(); ++current) {
        if (current && timestamps[current] < timestamps[current - 1]) throw std::invalid_argument("timestamps must be monotonic");
        future = future < current + 1 ? current + 1 : future;
        while (future < timestamps.size() && timestamps[future] < timestamps[current] + horizon_ms) ++future;
        result[current] = future;
    }
    return result;
}

inline int direction_label(const std::uint64_t current_mid, const std::uint64_t future_mid,
                           const std::uint64_t threshold) {
    if (threshold == 0) throw std::invalid_argument("threshold must be positive");
    if (future_mid >= current_mid && future_mid - current_mid >= threshold) return 1;
    if (current_mid >= future_mid && current_mid - future_mid >= threshold) return -1;
    return 0;
}

}  // namespace llab::bybit_l2_labels
