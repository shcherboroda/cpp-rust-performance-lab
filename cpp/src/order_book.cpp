#include "llab/order_book.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace llab::order_book {
namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void hash_byte(std::uint64_t &hash, std::uint8_t byte) {
    hash ^= byte;
    hash *= kFnvPrime;
}

void hash_u64(std::uint64_t &hash, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        hash_byte(hash, static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint64_t mix_order_id(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

} // namespace

std::size_t StableOrderIdHash::operator()(OrderId order_id) const noexcept {
    return static_cast<std::size_t>(mix_order_id(order_id));
}

OrderBook::OrderBook(std::size_t expected_order_capacity) {
    orders_.reserve(expected_order_capacity);
}

void OrderBook::apply(const Event &event) {
    switch (event.type) {
    case EventType::Add:
        add_order(event.order_id, event.side, event.price_ticks, event.quantity);
        return;
    case EventType::Cancel:
        subtract_from_order(event.order_id, event.quantity, false);
        return;
    case EventType::Execute:
        subtract_from_order(event.order_id, event.quantity, true);
        return;
    case EventType::Delete:
    case EventType::OrderDelete:
        remove_order(event.order_id);
        return;
    case EventType::Replace:
        if (event.replacement_order_id == event.order_id) {
            throw std::invalid_argument("replace requires a new order identifier");
        }
        remove_order(event.order_id);
        add_order(event.replacement_order_id, event.side, event.price_ticks, event.quantity);
        return;
    case EventType::OrderUpsert:
        upsert_order(event.order_id, event.side, event.price_ticks, event.quantity);
        return;
    }
    throw std::invalid_argument("unknown event type");
}

std::optional<Level> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    const auto &[price, quantity] = *bids_.rbegin();
    return Level{price, quantity};
}

std::optional<Level> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    const auto &[price, quantity] = *asks_.begin();
    return Level{price, quantity};
}

std::optional<Quantity> OrderBook::level_quantity(Side side, Price price_ticks) const {
    const auto &levels = levels_for(side);
    const auto it = levels.find(price_ticks);
    return it == levels.end() ? std::nullopt : std::optional<Quantity>{it->second};
}

std::size_t OrderBook::live_order_count() const noexcept {
    return orders_.size();
}

std::size_t OrderBook::order_index_capacity() const noexcept {
    return orders_.bucket_count();
}

std::uint64_t OrderBook::state_digest() const {
    std::uint64_t hash = kFnvOffsetBasis;
    hash_u64(hash, static_cast<std::uint64_t>(orders_.size()));

    for (const auto *levels : std::array{&bids_, &asks_}) {
        hash_u64(hash, static_cast<std::uint64_t>(levels->size()));
        for (const auto &[price, quantity] : *levels) {
            hash_u64(hash, price);
            hash_u64(hash, quantity);
        }
    }

    std::vector<OrderId> order_ids;
    order_ids.reserve(orders_.size());
    for (const auto &[order_id, unused] : orders_) {
        (void)unused;
        order_ids.push_back(order_id);
    }
    std::sort(order_ids.begin(), order_ids.end());
    for (const OrderId order_id : order_ids) {
        const Order &order = orders_.at(order_id);
        hash_u64(hash, order_id);
        hash_byte(hash, static_cast<std::uint8_t>(order.side));
        hash_u64(hash, order.price_ticks);
        hash_u64(hash, order.remaining_quantity);
    }
    return hash;
}

OrderBook::Levels &OrderBook::levels_for(Side side) {
    return side == Side::Bid ? bids_ : asks_;
}

const OrderBook::Levels &OrderBook::levels_for(Side side) const {
    return side == Side::Bid ? bids_ : asks_;
}

void OrderBook::add_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity) {
    if (order_id == 0 || quantity == 0 || orders_.contains(order_id)) {
        throw std::invalid_argument("invalid add order");
    }
    add_to_level(side, price_ticks, quantity);
    orders_.emplace(order_id, Order{side, price_ticks, quantity});
}

void OrderBook::remove_order(OrderId order_id) {
    const auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        throw std::invalid_argument("unknown order identifier");
    }
    subtract_from_level(it->second.side, it->second.price_ticks, it->second.remaining_quantity);
    orders_.erase(it);
}

void OrderBook::subtract_from_order(OrderId order_id, Quantity quantity, bool allow_full_removal) {
    const auto it = orders_.find(order_id);
    if (it == orders_.end() || quantity == 0 || quantity > it->second.remaining_quantity ||
        (!allow_full_removal && quantity == it->second.remaining_quantity)) {
        throw std::invalid_argument("invalid order reduction");
    }
    const Order order = it->second;
    subtract_from_level(order.side, order.price_ticks, quantity);
    if (quantity == order.remaining_quantity) {
        orders_.erase(it);
    } else {
        it->second.remaining_quantity -= quantity;
    }
}

void OrderBook::upsert_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity) {
    if (order_id == 0 || quantity == 0) {
        throw std::invalid_argument("invalid order upsert");
    }
    const auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        subtract_from_level(it->second.side, it->second.price_ticks, it->second.remaining_quantity);
        it->second = Order{side, price_ticks, quantity};
    } else {
        orders_.emplace(order_id, Order{side, price_ticks, quantity});
    }
    add_to_level(side, price_ticks, quantity);
}

void OrderBook::add_to_level(Side side, Price price_ticks, Quantity quantity) {
    auto &level = levels_for(side)[price_ticks];
    if (UINT64_MAX - level < quantity) {
        throw std::overflow_error("price-level quantity overflow");
    }
    level += quantity;
}

void OrderBook::subtract_from_level(Side side, Price price_ticks, Quantity quantity) {
    auto &levels = levels_for(side);
    const auto it = levels.find(price_ticks);
    if (it == levels.end() || quantity == 0 || quantity > it->second) {
        throw std::logic_error("invalid price-level reduction");
    }
    it->second -= quantity;
    if (it->second == 0) {
        levels.erase(it);
    }
}

} // namespace llab::order_book
