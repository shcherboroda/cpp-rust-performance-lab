#include "llab/bitmap_backshift_order_book.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>

namespace llab::bitmap_backshift_order_book {
OrderBook::OrderBook(std::size_t max, Price minimum, std::size_t count)
    : orders_(max), minimum_price_(minimum) {
    if (count != kPriceCount || minimum > std::numeric_limits<Price>::max() - (kPriceCount - 1))
        throw std::invalid_argument("v5 requires 256 valid prices");
}
void OrderBook::apply(const Event &e) {
    switch (e.type) {
    case EventType::Add:
        add(e.order_id, e.side, e.price_ticks, e.quantity);
        return;
    case EventType::Cancel:
        reduce(e.order_id, e.quantity, false);
        return;
    case EventType::Execute:
        reduce(e.order_id, e.quantity, true);
        return;
    case EventType::Delete:
    case EventType::OrderDelete:
        remove(e.order_id);
        return;
    case EventType::Replace:
        if (e.order_id == e.replacement_order_id)
            throw std::invalid_argument("replace id");
        remove(e.order_id);
        add(e.replacement_order_id, e.side, e.price_ticks, e.quantity);
        return;
    case EventType::OrderUpsert:
        upsert(e.order_id, e.side, e.price_ticks, e.quantity);
        return;
    }
}
std::optional<Level> OrderBook::best_bid() const {
    return best(Side::Bid);
}
std::optional<Level> OrderBook::best_ask() const {
    return best(Side::Ask);
}
std::size_t OrderBook::live_order_count() const noexcept {
    return live_;
}
std::size_t OrderBook::order_index_capacity() const noexcept {
    return orders_.capacity();
}
std::size_t OrderBook::order_index_total_probes() const noexcept {
    return orders_.probe_summary().total_probes;
}
std::size_t OrderBook::order_index_maximum_probes() const noexcept {
    return orders_.probe_summary().maximum_probes;
}
std::uint64_t OrderBook::state_digest() const {
    std::uint64_t h = 14695981039346656037ULL;
    auto b = [&](std::uint8_t x) { h = (h ^ x) * 1099511628211ULL; };
    auto u = [&](std::uint64_t x) {
        for (unsigned s = 0; s < 64; s += 8)
            b(static_cast<std::uint8_t>(x >> s));
    };
    u(live_);
    for (const auto *l : {&bids_, &asks_}) {
        std::size_t n = 0;
        for (auto q : *l)
            if (q)
                ++n;
        u(n);
        for (std::size_t i = 0; i < l->size(); ++i)
            if ((*l)[i]) {
                u(minimum_price_ + i);
                u((*l)[i]);
            }
    }
    std::vector<std::pair<OrderId, Order>> entries;
    entries.reserve(orders_.size());
    orders_.for_each([&](OrderId id, Order order) { entries.emplace_back(id, order); });
    std::sort(entries.begin(), entries.end(),
              [](const auto &left, const auto &right) { return left.first < right.first; });
    for (const auto &[id, order] : entries) {
        u(id);
        b(static_cast<std::uint8_t>(order.side));
        u(order.price);
        u(order.quantity);
    }
    return h;
}
std::size_t OrderBook::offset(Price p) const {
    if (p < minimum_price_ || p - minimum_price_ >= kPriceCount)
        throw std::invalid_argument("price");
    return static_cast<std::size_t>(p - minimum_price_);
}
std::array<Quantity, OrderBook::kPriceCount> &OrderBook::ladder(Side s) {
    return s == Side::Bid ? bids_ : asks_;
}
const std::array<Quantity, OrderBook::kPriceCount> &OrderBook::ladder(Side s) const {
    return s == Side::Bid ? bids_ : asks_;
}
std::array<std::uint64_t, OrderBook::kBitmapWords> &OrderBook::occupancy(Side s) {
    return s == Side::Bid ? bid_occupied_ : ask_occupied_;
}
const std::array<std::uint64_t, OrderBook::kBitmapWords> &OrderBook::occupancy(Side s) const {
    return s == Side::Bid ? bid_occupied_ : ask_occupied_;
}
std::optional<Level> OrderBook::best(Side side) const {
    const auto &map = occupancy(side);
    std::size_t index = kPriceCount;
    if (side == Side::Bid) {
        for (std::size_t word = kBitmapWords; word-- > 0;) {
            auto bits = map[word];
            if (bits) {
                index = word * 64 + (63U - std::countl_zero(bits));
                break;
            }
        }
    } else {
        for (std::size_t word = 0; word < kBitmapWords; ++word) {
            auto bits = map[word];
            if (bits) {
                index = word * 64 + std::countr_zero(bits);
                break;
            }
        }
    }
    if (index == kPriceCount)
        return std::nullopt;
    return Level{minimum_price_ + index, ladder(side)[index]};
}
void OrderBook::add(OrderId id, Side side, Price p, Quantity q) {
    if (!id || !q)
        throw std::invalid_argument("add");
    if (!orders_.insert(id, {side, p, q}))
        throw std::invalid_argument("index");
    add_level(side, p, q);
    ++live_;
}
void OrderBook::remove(OrderId id) {
    const auto *order = orders_.find(id);
    if (!order)
        throw std::invalid_argument("id");
    const auto o = *order;
    subtract_level(o.side, o.price, o.quantity);
    static_cast<void>(orders_.erase(id));
    --live_;
}
void OrderBook::reduce(OrderId id, Quantity q, bool full) {
    auto *o = orders_.find(id);
    if (!o)
        throw std::invalid_argument("id");
    if (!q || q > o->quantity || (!full && q == o->quantity))
        throw std::invalid_argument("reduce");
    subtract_level(o->side, o->price, q);
    if (q == o->quantity) {
        static_cast<void>(orders_.erase(id));
        --live_;
    } else
        o->quantity -= q;
}
void OrderBook::upsert(OrderId id, Side side, Price p, Quantity q) {
    if (!id || !q)
        throw std::invalid_argument("upsert");
    auto *o = orders_.find(id);
    if (!o) {
        add(id, side, p, q);
        return;
    }
    subtract_level(o->side, o->price, o->quantity);
    add_level(side, p, q);
    *o = {side, p, q};
}
void OrderBook::add_level(Side side, Price p, Quantity q) {
    const auto i = offset(p);
    auto &level = ladder(side)[i];
    if (std::numeric_limits<Quantity>::max() - level < q)
        throw std::overflow_error("overflow");
    if (level == 0)
        occupancy(side)[i / 64] |= std::uint64_t{1} << (i % 64);
    level += q;
}
void OrderBook::subtract_level(Side side, Price p, Quantity q) {
    const auto i = offset(p);
    auto &level = ladder(side)[i];
    if (!q || q > level)
        throw std::logic_error("level");
    level -= q;
    if (level == 0)
        occupancy(side)[i / 64] &= ~(std::uint64_t{1} << (i % 64));
}
} // namespace llab::bitmap_backshift_order_book
