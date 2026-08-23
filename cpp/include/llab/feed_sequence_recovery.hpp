#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace llab {

enum class FeedRecoveryState : std::uint8_t { Live, AwaitingSnapshot, Snapshotting };
enum class FeedIncrementalResult : std::uint8_t { Apply, IgnoreStale, Gap, Buffered, BufferFull };

template <typename Event> class FeedSequenceRecovery {
  public:
    explicit FeedSequenceRecovery(std::size_t maximum_replay_events)
        : replay_(maximum_replay_events), replay_size_(0) {
    }

    [[nodiscard]] FeedRecoveryState state() const noexcept {
        return state_;
    }

    [[nodiscard]] std::optional<std::uint64_t> expected_sequence() const noexcept {
        return state_ == FeedRecoveryState::Live ? std::optional{expected_} : std::nullopt;
    }

    FeedIncrementalResult on_incremental(std::uint64_t sequence, const Event &event,
                                         std::vector<Event> &ready) {
        if (state_ == FeedRecoveryState::AwaitingSnapshot)
            return FeedIncrementalResult::Gap;
        if (state_ == FeedRecoveryState::Snapshotting) {
            if (sequence <= snapshot_last_)
                return FeedIncrementalResult::IgnoreStale;
            if (replay_size_ == replay_.size()) {
                state_ = FeedRecoveryState::AwaitingSnapshot;
                return FeedIncrementalResult::BufferFull;
            }
            replay_[replay_size_++] = {sequence, event};
            return FeedIncrementalResult::Buffered;
        }
        if (sequence < expected_)
            return FeedIncrementalResult::IgnoreStale;
        if (sequence > expected_) {
            state_ = FeedRecoveryState::AwaitingSnapshot;
            return FeedIncrementalResult::Gap;
        }
        ready.push_back(event);
        ++expected_;
        return FeedIncrementalResult::Apply;
    }

    void begin_snapshot(std::uint64_t last_sequence) noexcept {
        state_ = FeedRecoveryState::Snapshotting;
        snapshot_last_ = last_sequence;
        replay_size_ = 0;
    }

    [[nodiscard]] bool finish_snapshot(std::vector<Event> &ready) {
        if (state_ != FeedRecoveryState::Snapshotting)
            return false;
        std::sort(
            replay_.begin(), replay_.begin() + static_cast<std::ptrdiff_t>(replay_size_),
            [](const auto &left, const auto &right) { return left.sequence < right.sequence; });
        const auto expected = snapshot_last_ + 1;
        std::size_t first = 0;
        while (first < replay_size_ && replay_[first].sequence < expected)
            ++first;
        std::uint64_t next = expected;
        for (std::size_t i = first; i < replay_size_; ++i) {
            if (replay_[i].sequence < next)
                continue;
            if (replay_[i].sequence > next) {
                state_ = FeedRecoveryState::AwaitingSnapshot;
                replay_size_ = 0;
                return false;
            }
            ++next;
        }
        std::uint64_t emit_sequence = expected;
        for (std::size_t i = first; i < replay_size_; ++i)
            if (replay_[i].sequence == emit_sequence) {
                ready.push_back(replay_[i].event);
                ++emit_sequence;
            }
        replay_size_ = 0;
        expected_ = next;
        state_ = FeedRecoveryState::Live;
        return true;
    }

  private:
    struct Buffered {
        std::uint64_t sequence = 0;
        Event event{};
    };

    std::vector<Buffered> replay_;
    std::size_t replay_size_ = 0;
    FeedRecoveryState state_ = FeedRecoveryState::AwaitingSnapshot;
    std::uint64_t expected_ = 0;
    std::uint64_t snapshot_last_ = 0;
};

} // namespace llab
