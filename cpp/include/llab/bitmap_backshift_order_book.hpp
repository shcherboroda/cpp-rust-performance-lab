#pragma once

#include "llab/fixed_order_index.hpp"
#include "llab/parity_order_book.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace llab::bitmap_backshift_order_book {
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
    [[nodiscard]] std::size_t order_index_total_probes() const noexcept;
    [[nodiscard]] std::size_t order_index_maximum_probes() const noexcept;
    [[nodiscard]] std::uint64_t state_digest() const;

  private:
    struct Order {
        Side side;
        Price price;
        Quantity quantity;
    };
    static constexpr std::size_t kPriceCount = 256;
    static constexpr std::size_t kBitmapWords = kPriceCount / 64;
    FixedOrderIndex<Order> orders_;
    std::array<Quantity, kPriceCount> bids_{};
    std::array<Quantity, kPriceCount> asks_{};
    std::array<std::uint64_t, kBitmapWords> bid_occupied_{};
    std::array<std::uint64_t, kBitmapWords> ask_occupied_{};
    Price minimum_price_;
    std::size_t live_ = 0;

    [[nodiscard]] std::size_t offset(Price price) const;
    [[nodiscard]] std::array<Quantity, kPriceCount> &ladder(Side side);
    [[nodiscard]] const std::array<Quantity, kPriceCount> &ladder(Side side) const;
    [[nodiscard]] std::array<std::uint64_t, kBitmapWords> &occupancy(Side side);
    [[nodiscard]] const std::array<std::uint64_t, kBitmapWords> &occupancy(Side side) const;
    [[nodiscard]] std::optional<Level> best(Side side) const;
    void add(OrderId id, Side side, Price price, Quantity quantity);
    void remove(OrderId id);
    void reduce(OrderId id, Quantity quantity, bool full);
    void upsert(OrderId id, Side side, Price price, Quantity quantity);
    void add_level(Side side, Price price, Quantity quantity);
    void subtract_level(Side side, Price price, Quantity quantity);
};
} // namespace llab::bitmap_backshift_order_book
