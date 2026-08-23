#pragma once

#include "llab/parity_order_book.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace llab {

template <typename Value> class FixedOrderIndex {
  public:
    struct ProbeSummary {
        std::size_t occupied = 0;
        std::size_t total_probes = 0;
        std::size_t maximum_probes = 0;
    };
    explicit FixedOrderIndex(std::size_t maximum_live_entries);

    [[nodiscard]] bool insert(parity_order_book::OrderId id, const Value &value);
    [[nodiscard]] Value *find(parity_order_book::OrderId id);
    [[nodiscard]] const Value *find(parity_order_book::OrderId id) const;
    [[nodiscard]] std::optional<Value> erase(parity_order_book::OrderId id);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;

    template <typename Visitor> void for_each(Visitor &&visitor) const;
    [[nodiscard]] ProbeSummary probe_summary() const noexcept;

  private:
    struct Slot {
        parity_order_book::OrderId id = 0;
        Value value{};
        bool occupied = false;
    };

    std::vector<Slot> slots_;
    std::size_t maximum_live_entries_ = 0;
    std::size_t size_ = 0;

    [[nodiscard]] std::size_t home(parity_order_book::OrderId id) const noexcept;
    [[nodiscard]] std::size_t find_slot(parity_order_book::OrderId id) const noexcept;
    [[nodiscard]] static std::size_t distance(std::size_t from, std::size_t to,
                                              std::size_t mask) noexcept;
    void erase_at(std::size_t index) noexcept;
};

namespace detail {
inline std::uint64_t mix_fixed_order_id(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

inline std::size_t fixed_order_index_capacity(std::size_t maximum_live_entries) {
    if (maximum_live_entries > std::numeric_limits<std::size_t>::max() / 2)
        throw std::invalid_argument("fixed order index capacity overflow");
    const std::size_t target = std::max<std::size_t>(8, maximum_live_entries * 2);
    std::size_t result = 8;
    while (result < target)
        result <<= 1U;
    return result;
}
} // namespace detail

// Templates are defined in this header: the compiler must see the method body
// when it instantiates FixedOrderIndex for a caller's Value type.
template <typename Value>
FixedOrderIndex<Value>::FixedOrderIndex(std::size_t maximum_live_entries)
    : slots_(detail::fixed_order_index_capacity(maximum_live_entries)),
      maximum_live_entries_(maximum_live_entries) {
}

template <typename Value>
bool FixedOrderIndex<Value>::insert(parity_order_book::OrderId id, const Value &value) {
    if (id == 0 || size_ == maximum_live_entries_)
        return false;

    const std::size_t mask = slots_.size() - 1;
    for (std::size_t index = home(id), step = 0; step < slots_.size();
         ++step, index = (index + 1) & mask) {
        Slot &slot = slots_[index];
        if (!slot.occupied) {
            slot = Slot{id, value, true};
            ++size_;
            return true;
        }
        if (slot.id == id)
            return false;
    }
    return false;
}

template <typename Value> Value *FixedOrderIndex<Value>::find(parity_order_book::OrderId id) {
    const std::size_t index = find_slot(id);
    return index == slots_.size() ? nullptr : &slots_[index].value;
}

template <typename Value>
const Value *FixedOrderIndex<Value>::find(parity_order_book::OrderId id) const {
    const std::size_t index = find_slot(id);
    return index == slots_.size() ? nullptr : &slots_[index].value;
}

template <typename Value>
std::optional<Value> FixedOrderIndex<Value>::erase(parity_order_book::OrderId id) {
    const std::size_t index = find_slot(id);
    if (index == slots_.size())
        return std::nullopt;
    Value value = slots_[index].value;
    erase_at(index);
    --size_;
    return value;
}

template <typename Value> std::size_t FixedOrderIndex<Value>::size() const noexcept {
    return size_;
}

template <typename Value> std::size_t FixedOrderIndex<Value>::capacity() const noexcept {
    return slots_.size();
}

template <typename Value>
template <typename Visitor>
void FixedOrderIndex<Value>::for_each(Visitor &&visitor) const {
    for (const auto &slot : slots_)
        if (slot.occupied)
            visitor(slot.id, slot.value);
}

template <typename Value>
typename FixedOrderIndex<Value>::ProbeSummary
FixedOrderIndex<Value>::probe_summary() const noexcept {
    ProbeSummary summary;
    const auto mask = slots_.size() - 1;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (!slots_[index].occupied)
            continue;
        const std::size_t probes = distance(home(slots_[index].id), index, mask) + 1;
        ++summary.occupied;
        summary.total_probes += probes;
        summary.maximum_probes = std::max(summary.maximum_probes, probes);
    }
    return summary;
}

template <typename Value>
std::size_t FixedOrderIndex<Value>::home(parity_order_book::OrderId id) const noexcept {
    return static_cast<std::size_t>(detail::mix_fixed_order_id(id)) & (slots_.size() - 1);
}

template <typename Value>
std::size_t FixedOrderIndex<Value>::find_slot(parity_order_book::OrderId id) const noexcept {
    if (id == 0)
        return slots_.size();
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t index = home(id), step = 0; step < slots_.size();
         ++step, index = (index + 1) & mask) {
        const Slot &slot = slots_[index];
        if (!slot.occupied)
            return slots_.size();
        if (slot.id == id)
            return index;
    }
    return slots_.size();
}

template <typename Value>
std::size_t FixedOrderIndex<Value>::distance(std::size_t from, std::size_t to,
                                             std::size_t mask) noexcept {
    return (to - from) & mask;
}

template <typename Value> void FixedOrderIndex<Value>::erase_at(std::size_t index) noexcept {
    const std::size_t mask = slots_.size() - 1;
    std::size_t hole = index;
    std::size_t next = (hole + 1) & mask;
    while (slots_[next].occupied) {
        const std::size_t entry_home = home(slots_[next].id);
        if (distance(entry_home, next, mask) >= distance(entry_home, hole, mask)) {
            slots_[hole] = slots_[next];
            hole = next;
        }
        next = (next + 1) & mask;
    }
    slots_[hole].occupied = false;
}

} // namespace llab
