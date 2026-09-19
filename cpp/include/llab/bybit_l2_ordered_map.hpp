#pragma once
#include "llab/bybit_l2_order_book.hpp"
#include <map>
namespace llab::bybit_l2 {
class OrderedMapBook {
  public:
    [[nodiscard]] bool apply_snapshot(std::uint64_t u, const std::vector<Level>& bids, const std::vector<Level>& asks) { bids_.clear(); asks_.clear(); if (u==0 || !load(bids,Side::Bid) || !load(asks,Side::Ask)) return valid_=false; update_id_=u; return valid_=true; }
    [[nodiscard]] bool apply_delta(std::uint64_t u, const std::vector<Change>& changes) { if(!valid_||u!=update_id_+1) return valid_=false; for(auto c:changes) change(c); update_id_=u; return true; }
    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] std::optional<Level> best_bid() const { return bids_.empty()?std::nullopt:std::optional{Level{bids_.rbegin()->first,bids_.rbegin()->second}}; }
    [[nodiscard]] std::optional<Level> best_ask() const { return asks_.empty()?std::nullopt:std::optional{Level{asks_.begin()->first,asks_.begin()->second}}; }
  private:
    std::map<Price,Quantity> bids_,asks_; std::uint64_t update_id_{}; bool valid_{};
    bool load(const std::vector<Level>& levels, Side side) { for(auto l:levels) { if(!l.quantity) return false; (side==Side::Bid?bids_:asks_)[l.price]=l.quantity; } return true; }
    void change(Change c) { auto& map=c.side==Side::Bid?bids_:asks_; if(c.quantity) map[c.price]=c.quantity; else map.erase(c.price); }
};
} // namespace llab::bybit_l2
