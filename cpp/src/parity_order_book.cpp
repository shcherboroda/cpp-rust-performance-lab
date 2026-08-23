#include "llab/parity_order_book.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace llab::parity_order_book {
namespace {
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t mix_order_id(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

std::size_t table_capacity(std::size_t maximum_live_orders) {
    if (maximum_live_orders > (std::numeric_limits<std::size_t>::max() / 2))
        throw std::invalid_argument("capacity overflow");
    std::size_t capacity = 8;
    const std::size_t target = std::max<std::size_t>(8, maximum_live_orders * 2);
    while (capacity < target)
        capacity <<= 1U;
    return capacity;
}

void hash_byte(std::uint64_t &hash, std::uint8_t byte) {
    hash = (hash ^ byte) * kFnvPrime;
}
void hash_u64(std::uint64_t &hash, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        hash_byte(hash, static_cast<std::uint8_t>(value >> shift));
}
} // namespace

OrderBook::OrderBook(std::size_t maximum_live_orders)
    : slots_(table_capacity(maximum_live_orders)) {
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
        if (event.order_id == event.replacement_order_id)
            throw std::invalid_argument("replace requires new id");
        remove_order(event.order_id);
        add_order(event.replacement_order_id, event.side, event.price_ticks, event.quantity);
        return;
    case EventType::OrderUpsert:
        upsert_order(event.order_id, event.side, event.price_ticks, event.quantity);
        return;
    }
}

std::optional<Level> OrderBook::best_bid() const {
    return bids_.empty() ? std::nullopt : std::optional<Level>{bids_.back()};
}
std::optional<Level> OrderBook::best_ask() const {
    return asks_.empty() ? std::nullopt : std::optional<Level>{asks_.front()};
}
std::optional<Quantity> OrderBook::level_quantity(Side side, Price price) const {
    const auto &levels = levels_for(side);
    const auto it =
        std::lower_bound(levels.begin(), levels.end(), price,
                         [](const Level &level, Price value) { return level.price_ticks < value; });
    return it != levels.end() && it->price_ticks == price ? std::optional<Quantity>{it->quantity}
                                                          : std::nullopt;
}
std::size_t OrderBook::live_order_count() const noexcept {
    return live_order_count_;
}
std::size_t OrderBook::order_index_capacity() const noexcept {
    return slots_.size();
}

std::uint64_t OrderBook::state_digest() const {
    std::uint64_t hash = kFnvOffsetBasis;
    hash_u64(hash, live_order_count_);
    for (const auto *levels : std::array{&bids_, &asks_}) {
        hash_u64(hash, levels->size());
        for (const Level &level : *levels) {
            hash_u64(hash, level.price_ticks);
            hash_u64(hash, level.quantity);
        }
    }
    std::vector<OrderId> ids;
    ids.reserve(live_order_count_);
    for (const Slot &slot : slots_)
        if (slot.state == SlotState::Occupied)
            ids.push_back(slot.order_id);
    std::sort(ids.begin(), ids.end());
    for (OrderId id : ids) {
        const Order &order = require_slot(id).order;
        hash_u64(hash, id);
        hash_byte(hash, static_cast<std::uint8_t>(order.side));
        hash_u64(hash, order.price_ticks);
        hash_u64(hash, order.remaining_quantity);
    }
    return hash;
}

std::size_t OrderBook::find_slot(OrderId id) const {
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t step = 0, index = static_cast<std::size_t>(mix_order_id(id)) & mask;
         step < slots_.size(); ++step, index = (index + 1) & mask) {
        const Slot &slot = slots_[index];
        if (slot.state == SlotState::Empty)
            return slots_.size();
        if (slot.state == SlotState::Occupied && slot.order_id == id)
            return index;
    }
    return slots_.size();
}

std::size_t OrderBook::insertion_slot(OrderId id) const {
    const std::size_t mask = slots_.size() - 1;
    std::size_t first_tombstone = slots_.size();
    for (std::size_t step = 0, index = static_cast<std::size_t>(mix_order_id(id)) & mask;
         step < slots_.size(); ++step, index = (index + 1) & mask) {
        const Slot &slot = slots_[index];
        if (slot.state == SlotState::Empty)
            return first_tombstone == slots_.size() ? index : first_tombstone;
        if (slot.state == SlotState::Tombstone && first_tombstone == slots_.size())
            first_tombstone = index;
        if (slot.state == SlotState::Occupied && slot.order_id == id)
            return slots_.size();
    }
    return first_tombstone;
}

OrderBook::Slot &OrderBook::require_slot(OrderId id) {
    const auto index = find_slot(id);
    if (index == slots_.size())
        throw std::invalid_argument("unknown order id");
    return slots_[index];
}
const OrderBook::Slot &OrderBook::require_slot(OrderId id) const {
    const auto index = find_slot(id);
    if (index == slots_.size())
        throw std::invalid_argument("unknown order id");
    return slots_[index];
}
std::vector<Level> &OrderBook::levels_for(Side side) {
    return side == Side::Bid ? bids_ : asks_;
}
const std::vector<Level> &OrderBook::levels_for(Side side) const {
    return side == Side::Bid ? bids_ : asks_;
}

void OrderBook::add_order(OrderId id, Side side, Price price, Quantity quantity) {
    if (id == 0 || quantity == 0)
        throw std::invalid_argument("invalid add");
    const auto index = insertion_slot(id);
    if (index == slots_.size())
        throw std::invalid_argument("duplicate id or full index");
    add_to_level(side, price, quantity);
    slots_[index] = Slot{id, Order{side, price, quantity}, SlotState::Occupied};
    ++live_order_count_;
}
void OrderBook::remove_order(OrderId id) {
    Slot &slot = require_slot(id);
    subtract_from_level(slot.order.side, slot.order.price_ticks, slot.order.remaining_quantity);
    slot.state = SlotState::Tombstone;
    --live_order_count_;
}
void OrderBook::subtract_from_order(OrderId id, Quantity quantity, bool allow_full_removal) {
    Slot &slot = require_slot(id);
    if (quantity == 0 || quantity > slot.order.remaining_quantity ||
        (!allow_full_removal && quantity == slot.order.remaining_quantity))
        throw std::invalid_argument("invalid reduction");
    subtract_from_level(slot.order.side, slot.order.price_ticks, quantity);
    if (quantity == slot.order.remaining_quantity) {
        slot.state = SlotState::Tombstone;
        --live_order_count_;
    } else
        slot.order.remaining_quantity -= quantity;
}
void OrderBook::upsert_order(OrderId id, Side side, Price price, Quantity quantity) {
    if (id == 0 || quantity == 0)
        throw std::invalid_argument("invalid upsert");
    const auto index = find_slot(id);
    if (index == slots_.size()) {
        add_order(id, side, price, quantity);
        return;
    }
    Slot &slot = slots_[index];
    subtract_from_level(slot.order.side, slot.order.price_ticks, slot.order.remaining_quantity);
    add_to_level(side, price, quantity);
    slot.order = Order{side, price, quantity};
}
void OrderBook::add_to_level(Side side, Price price, Quantity quantity) {
    auto &levels = levels_for(side);
    const auto it =
        std::lower_bound(levels.begin(), levels.end(), price,
                         [](const Level &level, Price value) { return level.price_ticks < value; });
    if (it != levels.end() && it->price_ticks == price) {
        if (std::numeric_limits<Quantity>::max() - it->quantity < quantity)
            throw std::overflow_error("level overflow");
        it->quantity += quantity;
    } else
        levels.insert(it, Level{price, quantity});
}
void OrderBook::subtract_from_level(Side side, Price price, Quantity quantity) {
    auto &levels = levels_for(side);
    const auto it =
        std::lower_bound(levels.begin(), levels.end(), price,
                         [](const Level &level, Price value) { return level.price_ticks < value; });
    if (it == levels.end() || it->price_ticks != price || quantity == 0 || quantity > it->quantity)
        throw std::logic_error("invalid level reduction");
    it->quantity -= quantity;
    if (it->quantity == 0)
        levels.erase(it);
}

} // namespace llab::parity_order_book
