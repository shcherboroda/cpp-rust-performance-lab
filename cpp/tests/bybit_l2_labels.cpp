#include "llab/bybit_l2_labels.hpp"
#include <stdexcept>
#include <vector>
int main() {
    const std::vector<std::uint64_t> times{100, 150, 200, 250};
    const auto future = llab::bybit_l2_labels::first_future_indices(times, 100);
    if (future != std::vector<std::size_t>{2, 3, 4, 4}) throw std::runtime_error("first future boundary failed");
    if (llab::bybit_l2_labels::direction_label(100, 109, 10) != 0 ||
        llab::bybit_l2_labels::direction_label(100, 110, 10) != 1 ||
        llab::bybit_l2_labels::direction_label(100, 90, 10) != -1) throw std::runtime_error("neutral band failed");
    try { static_cast<void>(llab::bybit_l2_labels::first_future_indices(std::vector<std::uint64_t>{2, 1}, 1)); }
    catch (const std::invalid_argument&) { return 0; }
    throw std::runtime_error("timestamp regression accepted");
}
