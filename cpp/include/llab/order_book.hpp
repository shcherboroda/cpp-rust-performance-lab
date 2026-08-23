#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

namespace llab::order_book {

using OrderId = std::uint64_t;
using Price = std::uint64_t;
using Quantity = std::uint64_t;

enum class Side : std::uint8_t { Bid = 0, Ask = 1 };
enum class EventType : std::uint8_t {
    Add,
    Cancel,
    Execute,
    Delete,
    Replace,
    OrderUpsert,
    OrderDelete
};

// Same deterministic integer mixer is used by the Rust parity implementation.
// It is not a claim of collision-resistance against adversarial order identifiers.
struct StableOrderIdHash {
    [[nodiscard]] std::size_t operator()(OrderId order_id) const noexcept;
};

struct Event {
    EventType type;
    OrderId order_id = 0;
    OrderId replacement_order_id = 0;
    Side side = Side::Bid;
    Price price_ticks = 0;
    Quantity quantity = 0;
};

struct Level {
    Price price_ticks;
    Quantity quantity;

    constexpr bool operator==(const Level &) const = default;
};

class OrderBook {
  public:
    explicit OrderBook(std::size_t expected_order_capacity = 0);

    void apply(const Event &event);

    [[nodiscard]] std::optional<Level> best_bid() const;
    [[nodiscard]] std::optional<Level> best_ask() const;
    [[nodiscard]] std::optional<Quantity> level_quantity(Side side, Price price_ticks) const;
    [[nodiscard]] std::size_t live_order_count() const noexcept;
    [[nodiscard]] std::size_t order_index_capacity() const noexcept;
    [[nodiscard]] std::uint64_t state_digest() const;

  private:
    struct Order {
        Side side;
        Price price_ticks;
        Quantity remaining_quantity;
    };

    using Levels = std::map<Price, Quantity>;

    std::unordered_map<OrderId, Order, StableOrderIdHash> orders_;
    Levels bids_;
    Levels asks_;

    [[nodiscard]] Levels &levels_for(Side side);
    [[nodiscard]] const Levels &levels_for(Side side) const;
    void add_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity);
    void remove_order(OrderId order_id);
    void subtract_from_order(OrderId order_id, Quantity quantity, bool allow_full_removal);
    void upsert_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity);
    void add_to_level(Side side, Price price_ticks, Quantity quantity);
    void subtract_from_level(Side side, Price price_ticks, Quantity quantity);
};

} // namespace llab::order_book
