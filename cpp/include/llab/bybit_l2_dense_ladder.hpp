#pragma once
#include "llab/bybit_l2_order_book.hpp"
#include <vector>

namespace llab::bybit_l2 {
class DenseLadderBook {
  public:
    DenseLadderBook(Price minimum_price, std::size_t price_count)
        : minimum_(minimum_price), bids_(price_count), asks_(price_count) {}
    [[nodiscard]] bool apply_snapshot(std::uint64_t u, const std::vector<Level>& bids, const std::vector<Level>& asks) {
        clear(); if (u == 0 || !apply_all(bids, Side::Bid) || !apply_all(asks, Side::Ask)) { valid_ = false; return false; }
        update_id_ = u; valid_ = true; return true;
    }
    [[nodiscard]] bool apply_delta(std::uint64_t u, const std::vector<Change>& changes) {
        if (!valid_ || u != update_id_ + 1) { valid_ = false; return false; }
        for (const auto& c : changes) if (!set(c)) { valid_ = false; return false; }
        update_id_ = u; return true;
    }
    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] std::optional<Level> best_bid() const { for (std::size_t i=bids_.size(); i-- > 0;) if (bids_[i]) return Level{minimum_+i,bids_[i]}; return std::nullopt; }
    [[nodiscard]] std::optional<Level> best_ask() const { for (std::size_t i=0;i<asks_.size();++i) if (asks_[i]) return Level{minimum_+i,asks_[i]}; return std::nullopt; }
  private:
    Price minimum_; std::vector<Quantity> bids_, asks_; std::uint64_t update_id_{}; bool valid_{};
    void clear() { std::fill(bids_.begin(), bids_.end(), 0); std::fill(asks_.begin(), asks_.end(), 0); }
    bool set(Change c) { if (c.price < minimum_ || c.price - minimum_ >= bids_.size()) return false; (c.side == Side::Bid ? bids_ : asks_)[c.price-minimum_] = c.quantity; return true; }
    bool apply_all(const std::vector<Level>& levels, Side side) { for (auto l:levels) if (l.quantity == 0 || !set({side,l.price,l.quantity})) return false; return true; }
};
} // namespace llab::bybit_l2
