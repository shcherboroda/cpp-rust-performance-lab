#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace llab::parity_order_book {

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

struct Event {
    OrderId order_id = 0;
    OrderId replacement_order_id = 0;
    Price price_ticks = 0;
    Quantity quantity = 0;
    EventType type = EventType::Add;
    Side side = Side::Bid;

    constexpr Event() = default;
    constexpr Event(EventType event_type, OrderId id = 0, OrderId replacement_id = 0,
                    Side event_side = Side::Bid, Price price = 0, Quantity event_quantity = 0)
        : order_id(id), replacement_order_id(replacement_id), price_ticks(price),
          quantity(event_quantity), type(event_type), side(event_side) {
    }
};

static_assert(sizeof(Event) == 40, "parity event layout must be 40 bytes");

struct Level {
    Price price_ticks;
    Quantity quantity;
    constexpr bool operator==(const Level &) const = default;
};

class OrderBook {
  public:
    explicit OrderBook(std::size_t maximum_live_orders);
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
    enum class SlotState : std::uint8_t { Empty, Occupied, Tombstone };
    struct Slot {
        OrderId order_id = 0;
        Order order{};
        SlotState state = SlotState::Empty;
    };

    std::vector<Slot> slots_;
    std::vector<Level> bids_;
    std::vector<Level> asks_;
    std::size_t live_order_count_ = 0;

    [[nodiscard]] std::size_t find_slot(OrderId order_id) const;
    [[nodiscard]] std::size_t insertion_slot(OrderId order_id) const;
    [[nodiscard]] Slot &require_slot(OrderId order_id);
    [[nodiscard]] const Slot &require_slot(OrderId order_id) const;
    [[nodiscard]] std::vector<Level> &levels_for(Side side);
    [[nodiscard]] const std::vector<Level> &levels_for(Side side) const;
    void add_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity);
    void remove_order(OrderId order_id);
    void subtract_from_order(OrderId order_id, Quantity quantity, bool allow_full_removal);
    void upsert_order(OrderId order_id, Side side, Price price_ticks, Quantity quantity);
    void add_to_level(Side side, Price price_ticks, Quantity quantity);
    void subtract_from_level(Side side, Price price_ticks, Quantity quantity);
};

} // namespace llab::parity_order_book
