#include "llab/bitmap_backshift_order_book.hpp"

#include <cstdint>
#include <iostream>

int main() {
    using namespace llab::bitmap_backshift_order_book;
    constexpr std::size_t maximum = 32'768;
    OrderBook book(maximum, 99'873, 256);
    for (std::uint64_t id = 1; id <= 16'384; ++id) {
        const auto side = id % 2 == 0 ? Side::Bid : Side::Ask;
        const auto price = side == Side::Bid ? 100'000 - id % 128 : 100'001 + id % 128;
        book.apply({EventType::Add, id, 0, side, price, 1});
    }
    std::cout << "occupancy=" << book.live_order_count()
              << " total_probes=" << book.order_index_total_probes()
              << " max_probes=" << book.order_index_maximum_probes() << '\n';
}
