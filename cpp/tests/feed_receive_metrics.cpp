#include "llab/feed_receive_metrics.hpp"

#include <stdexcept>

int main() {
    llab::FeedReceiveMetrics metrics;
    metrics.record_frame(llab::FeedFrameClass::Control, 10, 3);
    metrics.record_frame(llab::FeedFrameClass::Snapshot, 20, 5);
    metrics.record_frame(llab::FeedFrameClass::MarketUpdate, 100, 7);
    metrics.record_frame(llab::FeedFrameClass::MarketUpdate, 132, 9);
    metrics.record_queue_overflow();
    const auto snapshot = metrics.snapshot();
    if (snapshot.control_frames != 1 || snapshot.snapshots != 1 || snapshot.market_updates != 2 ||
        snapshot.queue_overflows != 1 || snapshot.interarrival[llab::FeedReceiveMetrics::bucket(32)] != 1 ||
        snapshot.callback_to_handoff[llab::FeedReceiveMetrics::bucket(7)] != 2 ||
        snapshot.callback_to_handoff[llab::FeedReceiveMetrics::bucket(9)] != 1) {
        throw std::runtime_error("feed receive metrics contract failed");
    }
}
