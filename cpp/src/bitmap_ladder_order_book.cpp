#include "llab/bitmap_ladder_order_book.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace llab::bitmap_ladder_order_book {
namespace {
std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27U)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31U);
}
std::size_t capacity(std::size_t n) {
    std::size_t c = 8, target = std::max<std::size_t>(8, n * 2);
    while (c < target)
        c <<= 1U;
    return c;
}
} // namespace

OrderBook::OrderBook(std::size_t max, Price minimum, std::size_t count)
    : slots_(capacity(max)), minimum_price_(minimum) {
    if (count != kPriceCount || minimum > std::numeric_limits<Price>::max() - (kPriceCount - 1))
        throw std::invalid_argument("v4 requires 256 valid prices");
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
    return slots_.size();
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
    std::vector<OrderId> ids;
    for (const auto &s : slots_)
        if (s.state == State::Occupied)
            ids.push_back(s.id);
    std::sort(ids.begin(), ids.end());
    for (auto id : ids) {
        const auto &s = slots_[find(id)];
        u(id);
        b(static_cast<std::uint8_t>(s.order.side()));
        u(minimum_price_ + s.order.offset());
        u(s.order.quantity);
    }
    return h;
}
std::size_t OrderBook::find(OrderId id) const {
    const auto m = slots_.size() - 1;
    for (std::size_t i = mix(id) & m, n = 0; n < slots_.size(); ++n, i = (i + 1) & m) {
        const auto &s = slots_[i];
        if (s.state == State::Empty)
            return slots_.size();
        if (s.state == State::Occupied && s.id == id)
            return i;
    }
    return slots_.size();
}
std::size_t OrderBook::insertion(OrderId id) const {
    const auto m = slots_.size() - 1;
    auto tomb = slots_.size();
    for (std::size_t i = mix(id) & m, n = 0; n < slots_.size(); ++n, i = (i + 1) & m) {
        const auto &s = slots_[i];
        if (s.state == State::Empty)
            return tomb == slots_.size() ? i : tomb;
        if (s.state == State::Tombstone && tomb == slots_.size())
            tomb = i;
        if (s.state == State::Occupied && s.id == id)
            return slots_.size();
    }
    return tomb;
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
    const auto price_offset = offset(p);
    auto i = insertion(id);
    if (i == slots_.size())
        throw std::invalid_argument("index");
    add_level(side, p, q);
    slots_[i] = {id, Order::make(side, price_offset, q), State::Occupied};
    ++live_;
}
void OrderBook::remove(OrderId id) {
    auto i = find(id);
    if (i == slots_.size())
        throw std::invalid_argument("id");
    auto o = slots_[i].order;
    subtract_level(o.side(), minimum_price_ + o.offset(), o.quantity);
    slots_[i].state = State::Tombstone;
    --live_;
}
void OrderBook::reduce(OrderId id, Quantity q, bool full) {
    auto i = find(id);
    if (i == slots_.size())
        throw std::invalid_argument("id");
    auto &o = slots_[i].order;
    if (!q || q > o.quantity || (!full && q == o.quantity))
        throw std::invalid_argument("reduce");
    subtract_level(o.side(), minimum_price_ + o.offset(), q);
    if (q == o.quantity) {
        slots_[i].state = State::Tombstone;
        --live_;
    } else
        o.quantity -= q;
}
void OrderBook::upsert(OrderId id, Side side, Price p, Quantity q) {
    if (!id || !q)
        throw std::invalid_argument("upsert");
    auto i = find(id);
    if (i == slots_.size()) {
        add(id, side, p, q);
        return;
    }
    auto &o = slots_[i].order;
    const auto price_offset = offset(p);
    subtract_level(o.side(), minimum_price_ + o.offset(), o.quantity);
    add_level(side, p, q);
    o = Order::make(side, price_offset, q);
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
} // namespace llab::bitmap_ladder_order_book
