#include "llab/bybit_l2_order_book.hpp"
#include <stdexcept>
#include <vector>
namespace { void require(bool v) { if (!v) throw std::runtime_error("Bybit L2 fixture failed"); } }
int main() {
    using namespace llab::bybit_l2;
    OrderBook book(50);
    require(book.apply_snapshot(10, {{10'000'000'000, 200'000'000}, {9'900'000'000, 100'000'000}}, {{10'100'000'000, 300'000'000}, {10'200'000'000, 100'000'000}}));
    require(book.apply_delta(11, {{Side::Bid, 10'000'000'000, 150'000'000}, {Side::Bid, 9'975'000'000, 50'000'000}, {Side::Ask, 10'100'000'000, 0}}));
    require(book.apply_delta(12, {{Side::Bid, 9'900'000'000, 0}, {Side::Ask, 10'050'000'000, 250'000'000}}));
    require(book.apply_snapshot(1, {{9'950'000'000, 400'000'000}}, {{10'050'000'000, 200'000'000}}));
    require(book.apply_delta(2, {{Side::Bid, 9'975'000'000, 100'000'000}, {Side::Ask, 10'050'000'000, 0}}));
    require(book.best_bid() == Level{9'975'000'000, 100'000'000} && !book.best_ask() && book.level_count() == 2);
    require(!book.apply_delta(4, {}) && !book.valid());
    OrderBook bounded(1);
    require(bounded.apply_snapshot(1, {{1, 1}}, {{2, 1}}));
    require(!bounded.apply_delta(2, {{Side::Bid, 2, 1}}) && !bounded.valid());
    OrderBook replacement(1);
    require(replacement.apply_snapshot(1, {{10, 1}}, {{20, 1}}));
    require(replacement.apply_delta(2, {{Side::Ask, 19, 1}, {Side::Ask, 20, 0}}));
    require(replacement.best_ask() == Level{19, 1});
    OrderBook atomic_failure(1);
    require(atomic_failure.apply_snapshot(1, {{10, 1}}, {{20, 1}}));
    require(!atomic_failure.apply_delta(2, {{Side::Bid, 10, 0}, {Side::Bid, 9, 1}, {Side::Bid, 8, 1}}));
    require(!atomic_failure.valid() && !atomic_failure.best_bid() && atomic_failure.level_count() == 0);
}
