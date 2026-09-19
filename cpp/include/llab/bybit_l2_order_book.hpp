#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace llab::bybit_l2 {

using Price = std::uint64_t;
using Quantity = std::uint64_t;
enum class Side : std::uint8_t { Bid, Ask };
struct Level { Price price; Quantity quantity; constexpr bool operator==(const Level&) const = default; };
struct Change { Side side; Price price; Quantity quantity; };

class OrderBook {
  public:
    explicit OrderBook(const std::size_t maximum_levels_per_side) : maximum_levels_(maximum_levels_per_side) {
        bids_.reserve(maximum_levels_); asks_.reserve(maximum_levels_);
    }
    [[nodiscard]] bool apply_snapshot(std::uint64_t update_id, const std::vector<Level>& bids,
                                      const std::vector<Level>& asks) {
        if (update_id == 0 || bids.size() > maximum_levels_ || asks.size() > maximum_levels_ || !valid_levels(bids, Side::Bid) || !valid_levels(asks, Side::Ask)) return invalidate();
        bids_ = bids; asks_ = asks; update_id_ = update_id; valid_ = true; return true;
    }
    [[nodiscard]] bool apply_delta(std::uint64_t update_id, const std::vector<Change>& changes) {
        if (!valid_ || update_id_ == std::numeric_limits<std::uint64_t>::max() || update_id != update_id_ + 1 || !delta_fits(changes)) return invalidate();
        // A Bybit delta is one atomic book state. At a subscribed depth it can
        // add a new edge level and remove an old one in either array order;
        // remove all zero-size levels first so a transient full vector does
        // not turn a valid replacement into a false capacity failure.
        for (const auto& change : changes) if (change.quantity == 0 && !apply_change(change)) return invalidate();
        for (const auto& change : changes) if (change.quantity != 0 && !apply_change(change)) return invalidate();
        update_id_ = update_id; return true;
    }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] std::optional<Level> best_bid() const { return !valid_ || bids_.empty() ? std::nullopt : std::optional{bids_.front()}; }
    [[nodiscard]] std::optional<Level> best_ask() const { return !valid_ || asks_.empty() ? std::nullopt : std::optional{asks_.front()}; }
    [[nodiscard]] std::size_t level_count() const noexcept { return bids_.size() + asks_.size(); }
    [[nodiscard]] Quantity depth_quantity(const Side side, const std::size_t depth) const noexcept {
        const auto& levels = side == Side::Bid ? bids_ : asks_;
        Quantity total = 0;
        for (std::size_t index = 0; index < std::min(depth, levels.size()); ++index) total += levels[index].quantity;
        return total;
    }
    [[nodiscard]] std::uint64_t state_digest() const {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto* levels : {&bids_, &asks_}) { hash_u64(hash, levels->size()); for (const auto& l : *levels) { hash_u64(hash, l.price); hash_u64(hash, l.quantity); } }
        return hash;
    }
  private:
    std::vector<Level> bids_, asks_; std::size_t maximum_levels_; std::uint64_t update_id_ = 0; bool valid_ = false;
    bool invalidate() { bids_.clear(); asks_.clear(); update_id_ = 0; valid_ = false; return false; }
    static void hash_u64(std::uint64_t& hash, std::uint64_t value) { for (int i = 0; i < 8; ++i) { hash ^= value & 0xff; hash *= 1099511628211ULL; value >>= 8; } }
    static bool valid_levels(const std::vector<Level>& levels, Side side) {
        for (std::size_t i = 0; i < levels.size(); ++i) if (levels[i].quantity == 0 || (i && (side == Side::Bid ? levels[i - 1].price <= levels[i].price : levels[i - 1].price >= levels[i].price))) return false;
        return true;
    }
    bool delta_fits(const std::vector<Change>& changes) const {
        std::size_t bid_count = bids_.size(), ask_count = asks_.size();
        for (std::size_t i = 0; i < changes.size(); ++i) {
            for (std::size_t earlier = 0; earlier < i; ++earlier)
                if (changes[earlier].side == changes[i].side && changes[earlier].price == changes[i].price) return false;
            const auto& levels = changes[i].side == Side::Bid ? bids_ : asks_;
            const bool exists = std::any_of(levels.begin(), levels.end(), [&change = changes[i]](const Level& level) { return level.price == change.price; });
            auto& count = changes[i].side == Side::Bid ? bid_count : ask_count;
            if (changes[i].quantity == 0 && exists) --count;
            if (changes[i].quantity != 0 && !exists) ++count;
        }
        return bid_count <= maximum_levels_ && ask_count <= maximum_levels_;
    }
    bool apply_change(const Change& change) {
        auto& levels = change.side == Side::Bid ? bids_ : asks_;
        const auto before = std::lower_bound(levels.begin(), levels.end(), change.price, [side = change.side](const Level& level, Price price) { return side == Side::Bid ? level.price > price : level.price < price; });
        if (before != levels.end() && before->price == change.price) { if (change.quantity == 0) levels.erase(before); else before->quantity = change.quantity; return true; }
        if (change.quantity == 0) return true;
        if (levels.size() == maximum_levels_) return false;
        levels.insert(before, {change.price, change.quantity}); return true;
    }
};
} // namespace llab::bybit_l2
