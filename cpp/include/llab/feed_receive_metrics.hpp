#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace llab {
enum class FeedFrameClass { Control, Snapshot, MarketUpdate };
constexpr std::size_t feed_metric_buckets = 64;

struct FeedReceiveSnapshot {
    std::uint64_t control_frames = 0, snapshots = 0, market_updates = 0;
    std::uint64_t queue_overflows = 0, oversized_frames = 0;
    std::array<std::uint64_t, feed_metric_buckets> interarrival{};
    std::array<std::uint64_t, feed_metric_buckets> callback_to_handoff{};
};

class FeedReceiveMetrics {
public:
    void record_frame(const FeedFrameClass frame, const std::uint64_t received_ns, const std::uint64_t handoff_ns) {
        callback_to_handoff_[bucket(handoff_ns)]++;
        switch (frame) {
            case FeedFrameClass::Control: control_frames_++; break;
            case FeedFrameClass::Snapshot: snapshots_++; break;
            case FeedFrameClass::MarketUpdate:
                if (previous_market_update_ns_) interarrival_[bucket(received_ns - *previous_market_update_ns_)]++;
                previous_market_update_ns_ = received_ns;
                market_updates_++;
        }
    }
    void record_queue_overflow() { queue_overflows_++; }
    void record_oversized_frame() { oversized_frames_++; }
    [[nodiscard]] FeedReceiveSnapshot snapshot() const { return {control_frames_, snapshots_, market_updates_, queue_overflows_, oversized_frames_, interarrival_, callback_to_handoff_}; }
    [[nodiscard]] static std::size_t bucket(const std::uint64_t ns) {
        return ns == 0 ? 0 : std::min<std::size_t>(63, 63 - std::countl_zero(ns));
    }
private:
    std::uint64_t control_frames_ = 0, snapshots_ = 0, market_updates_ = 0, queue_overflows_ = 0, oversized_frames_ = 0;
    std::optional<std::uint64_t> previous_market_update_ns_;
    std::array<std::uint64_t, feed_metric_buckets> interarrival_{}, callback_to_handoff_{};
};
}  // namespace llab
