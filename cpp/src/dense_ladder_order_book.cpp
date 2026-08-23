#include "llab/dense_ladder_order_book.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace llab::dense_ladder_order_book {
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

OrderBook::OrderBook(std::size_t maximum_live_orders, Price minimum_price, std::size_t price_count)
    : slots_(capacity(maximum_live_orders)), bids_(price_count), asks_(price_count),
      minimum_price_(minimum_price) {
    if (price_count == 0 || minimum_price > std::numeric_limits<Price>::max() - (price_count - 1))
        throw std::invalid_argument("invalid ladder range");
}
void OrderBook::apply(const Event &e) {
    switch (e.type) {
    case EventType::Add:
        add_order(e.order_id, e.side, e.price_ticks, e.quantity);
        return;
    case EventType::Cancel:
        reduce(e.order_id, e.quantity, false);
        return;
    case EventType::Execute:
        reduce(e.order_id, e.quantity, true);
        return;
    case EventType::Delete:
    case EventType::OrderDelete:
        remove_order(e.order_id);
        return;
    case EventType::Replace:
        if (e.order_id == e.replacement_order_id)
            throw std::invalid_argument("replace id");
        remove_order(e.order_id);
        add_order(e.replacement_order_id, e.side, e.price_ticks, e.quantity);
        return;
    case EventType::OrderUpsert:
        upsert(e.order_id, e.side, e.price_ticks, e.quantity);
        return;
    }
}
std::optional<Level> OrderBook::best_bid() const {
    return best_bid_ ? std::optional<Level>{{*best_bid_, bids_[offset(*best_bid_)]}} : std::nullopt;
}
std::optional<Level> OrderBook::best_ask() const {
    return best_ask_ ? std::optional<Level>{{*best_ask_, asks_[offset(*best_ask_)]}} : std::nullopt;
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
        b(static_cast<std::uint8_t>(s.order.side));
        u(s.order.price);
        u(s.order.quantity);
    }
    return h;
}
std::size_t OrderBook::find(OrderId id) const {
    const auto mask = slots_.size() - 1;
    for (std::size_t i = mix(id) & mask, n = 0; n < slots_.size(); ++n, i = (i + 1) & mask) {
        const auto &s = slots_[i];
        if (s.state == State::Empty)
            return slots_.size();
        if (s.state == State::Occupied && s.id == id)
            return i;
    }
    return slots_.size();
}
std::size_t OrderBook::insert_slot(OrderId id) const {
    const auto mask = slots_.size() - 1;
    auto tomb = slots_.size();
    for (std::size_t i = mix(id) & mask, n = 0; n < slots_.size(); ++n, i = (i + 1) & mask) {
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
OrderBook::Slot &OrderBook::require(OrderId id) {
    auto i = find(id);
    if (i == slots_.size())
        throw std::invalid_argument("unknown id");
    return slots_[i];
}
std::size_t OrderBook::offset(Price price) const {
    if (price < minimum_price_ || price - minimum_price_ >= bids_.size())
        throw std::invalid_argument("price outside ladder");
    return static_cast<std::size_t>(price - minimum_price_);
}
std::vector<Quantity> &OrderBook::ladder(Side s) {
    return s == Side::Bid ? bids_ : asks_;
}
const std::vector<Quantity> &OrderBook::ladder(Side s) const {
    return s == Side::Bid ? bids_ : asks_;
}
void OrderBook::add_order(OrderId id, Side side, Price price, Quantity q) {
    if (!id || !q)
        throw std::invalid_argument("add");
    auto i = insert_slot(id);
    if (i == slots_.size())
        throw std::invalid_argument("index");
    add_level(side, price, q);
    slots_[i] = {id, {side, price, q}, State::Occupied};
    ++live_;
}
void OrderBook::remove_order(OrderId id) {
    auto &s = require(id);
    subtract_level(s.order.side, s.order.price, s.order.quantity);
    s.state = State::Tombstone;
    --live_;
}
void OrderBook::reduce(OrderId id, Quantity q, bool full) {
    auto &s = require(id);
    if (!q || q > s.order.quantity || (!full && q == s.order.quantity))
        throw std::invalid_argument("reduce");
    subtract_level(s.order.side, s.order.price, q);
    if (q == s.order.quantity) {
        s.state = State::Tombstone;
        --live_;
    } else
        s.order.quantity -= q;
}
void OrderBook::upsert(OrderId id, Side side, Price price, Quantity q) {
    if (!id || !q)
        throw std::invalid_argument("upsert");
    auto i = find(id);
    if (i == slots_.size()) {
        add_order(id, side, price, q);
        return;
    }
    auto &s = slots_[i];
    subtract_level(s.order.side, s.order.price, s.order.quantity);
    add_level(side, price, q);
    s.order = {side, price, q};
}
void OrderBook::add_level(Side side, Price price, Quantity q) {
    auto &l = ladder(side);
    auto &i = l[offset(price)];
    if (std::numeric_limits<Quantity>::max() - i < q)
        throw std::overflow_error("overflow");
    i += q;
    if (side == Side::Bid) {
        if (!best_bid_ || price > *best_bid_)
            best_bid_ = price;
    } else if (!best_ask_ || price < *best_ask_)
        best_ask_ = price;
}
void OrderBook::subtract_level(Side side, Price price, Quantity q) {
    auto &l = ladder(side);
    auto &i = l[offset(price)];
    if (!q || q > i)
        throw std::logic_error("level");
    i -= q;
    if (i == 0 && ((side == Side::Bid && best_bid_ && price == *best_bid_) ||
                   (side == Side::Ask && best_ask_ && price == *best_ask_)))
        refresh_best(side);
}
void OrderBook::refresh_best(Side side) {
    const auto &l = ladder(side);
    if (side == Side::Bid) {
        best_bid_.reset();
        for (std::size_t i = l.size(); i-- > 0;)
            if (l[i]) {
                best_bid_ = minimum_price_ + i;
                break;
            }
    } else {
        best_ask_.reset();
        for (std::size_t i = 0; i < l.size(); ++i)
            if (l[i]) {
                best_ask_ = minimum_price_ + i;
                break;
            }
    }
}
} // namespace llab::dense_ladder_order_book
