#pragma once

#include "llab/parity_order_book.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace llab::bitmap_ladder_order_book {
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
        std::uint64_t side_and_offset = 0;
        Quantity quantity;

        static constexpr std::uint64_t kAskBit = std::uint64_t{1} << 63U;
        [[nodiscard]] static Order make(Side side, std::size_t offset, Quantity quantity) noexcept {
            return {static_cast<std::uint64_t>(offset) | (side == Side::Ask ? kAskBit : 0),
                    quantity};
        }
        [[nodiscard]] Side side() const noexcept {
            return side_and_offset & kAskBit ? Side::Ask : Side::Bid;
        }
        [[nodiscard]] std::size_t offset() const noexcept {
            return static_cast<std::size_t>(side_and_offset & ~kAskBit);
        }
    };
    static_assert(sizeof(Order) == 16);
    enum class State : unsigned char { Empty, Occupied, Tombstone };
    struct Slot {
        OrderId id = 0;
        Order order{};
        State state = State::Empty;
    };
    static constexpr std::size_t kPriceCount = 256;
    static constexpr std::size_t kBitmapWords = kPriceCount / 64;
    std::vector<Slot> slots_;
    std::array<Quantity, kPriceCount> bids_{};
    std::array<Quantity, kPriceCount> asks_{};
    std::array<std::uint64_t, kBitmapWords> bid_occupied_{};
    std::array<std::uint64_t, kBitmapWords> ask_occupied_{};
    Price minimum_price_;
    std::size_t live_ = 0;

    [[nodiscard]] std::size_t find(OrderId id) const;
    [[nodiscard]] std::size_t insertion(OrderId id) const;
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
} // namespace llab::bitmap_ladder_order_book
