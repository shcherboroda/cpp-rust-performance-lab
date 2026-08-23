#pragma once

#include "llab/parity_order_book.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace llab::dense_ladder_order_book {

using parity_order_book::Event;
using parity_order_book::EventType;
using parity_order_book::Level;
using parity_order_book::OrderId;
using parity_order_book::Price;
using parity_order_book::Quantity;
using parity_order_book::Side;

class OrderBook {
  public:
    OrderBook(std::size_t maximum_live_orders, Price minimum_price, std::size_t price_count);
    void apply(const Event &event);
    [[nodiscard]] std::optional<Level> best_bid() const;
    [[nodiscard]] std::optional<Level> best_ask() const;
    [[nodiscard]] std::size_t live_order_count() const noexcept;
    [[nodiscard]] std::size_t order_index_capacity() const noexcept;
    [[nodiscard]] std::uint64_t state_digest() const;

  private:
    struct Order {
        Side side;
        Price price;
        Quantity quantity;
    };
    enum class State : unsigned char { Empty, Occupied, Tombstone };
    struct Slot {
        OrderId id = 0;
        Order order{};
        State state = State::Empty;
    };
    std::vector<Slot> slots_;
    std::vector<Quantity> bids_, asks_;
    Price minimum_price_;
    std::optional<Price> best_bid_, best_ask_;
    std::size_t live_ = 0;

    [[nodiscard]] std::size_t find(OrderId id) const;
    [[nodiscard]] std::size_t insert_slot(OrderId id) const;
    [[nodiscard]] Slot &require(OrderId id);
    [[nodiscard]] std::size_t offset(Price price) const;
    [[nodiscard]] std::vector<Quantity> &ladder(Side side);
    [[nodiscard]] const std::vector<Quantity> &ladder(Side side) const;
    void add_order(OrderId id, Side side, Price price, Quantity quantity);
    void remove_order(OrderId id);
    void reduce(OrderId id, Quantity quantity, bool full);
    void upsert(OrderId id, Side side, Price price, Quantity quantity);
    void add_level(Side side, Price price, Quantity quantity);
    void subtract_level(Side side, Price price, Quantity quantity);
    void refresh_best(Side side);
};
} // namespace llab::dense_ladder_order_book
