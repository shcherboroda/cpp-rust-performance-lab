#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

namespace llab {

struct BboSnapshot {
    std::uint64_t bid_price = 0;
    std::uint64_t bid_quantity = 0;
    std::uint64_t ask_price = 0;
    std::uint64_t ask_quantity = 0;

    constexpr bool operator==(const BboSnapshot &) const = default;
};

/// A single-writer, multi-reader seqlock publication cell.
///
/// A reader receives either one internally consistent BBO snapshot or retries;
/// it never blocks the book writer. The caller must ensure exactly one writer.
template <std::size_t Alignment> class alignas(Alignment) BboSnapshotCellBase {
  public:
    void publish(BboSnapshot snapshot) noexcept {
        const auto starting_version = version_.fetch_add(1, std::memory_order_acq_rel);
        bid_price_.store(snapshot.bid_price, std::memory_order_relaxed);
        bid_quantity_.store(snapshot.bid_quantity, std::memory_order_relaxed);
        ask_price_.store(snapshot.ask_price, std::memory_order_relaxed);
        ask_quantity_.store(snapshot.ask_quantity, std::memory_order_relaxed);
        version_.store(starting_version + 2, std::memory_order_release);
    }

    [[nodiscard]] std::optional<BboSnapshot> try_read() const noexcept {
        const auto before = version_.load(std::memory_order_acquire);
        if (before & 1U)
            return std::nullopt;
        const BboSnapshot snapshot{bid_price_.load(std::memory_order_relaxed),
                                   bid_quantity_.load(std::memory_order_relaxed),
                                   ask_price_.load(std::memory_order_relaxed),
                                   ask_quantity_.load(std::memory_order_relaxed)};
        const auto after = version_.load(std::memory_order_acquire);
        return before == after ? std::optional{snapshot} : std::nullopt;
    }

  private:
    std::atomic<std::uint64_t> version_ = 0;
    std::atomic<std::uint64_t> bid_price_ = 0;
    std::atomic<std::uint64_t> bid_quantity_ = 0;
    std::atomic<std::uint64_t> ask_price_ = 0;
    std::atomic<std::uint64_t> ask_quantity_ = 0;
};

using BboSnapshotCell = BboSnapshotCellBase<64>;
using UnalignedBboSnapshotCell = BboSnapshotCellBase<alignof(std::uint64_t)>;

static_assert(alignof(BboSnapshotCell) == 64);
static_assert(sizeof(BboSnapshotCell) == 64);
static_assert(alignof(UnalignedBboSnapshotCell) == alignof(std::uint64_t));

} // namespace llab
