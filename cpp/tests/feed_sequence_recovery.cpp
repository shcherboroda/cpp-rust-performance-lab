#include "llab/feed_sequence_recovery.hpp"

#include <stdexcept>
#include <vector>

namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("feed recovery test failed");
}
} // namespace

int main() {
    llab::FeedSequenceRecovery<int> recovery(4);
    std::vector<int> ready;

    recovery.begin_snapshot(10);
    require(recovery.on_incremental(12, 12, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(11, 11, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(11, 111, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.finish_snapshot(ready));
    require((ready == std::vector<int>{11, 12}));
    require(recovery.expected_sequence() == 13);

    ready.clear();
    require(recovery.on_incremental(12, 12, ready) == llab::FeedIncrementalResult::IgnoreStale);
    require(recovery.on_incremental(14, 14, ready) == llab::FeedIncrementalResult::Gap);
    require(recovery.state() == llab::FeedRecoveryState::AwaitingSnapshot);

    recovery.begin_snapshot(20);
    require(recovery.on_incremental(22, 22, ready) == llab::FeedIncrementalResult::Buffered);
    require(!recovery.finish_snapshot(ready));
    require(recovery.state() == llab::FeedRecoveryState::AwaitingSnapshot);

    recovery.begin_snapshot(30);
    require(recovery.on_incremental(31, 31, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(32, 32, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(33, 33, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(34, 34, ready) == llab::FeedIncrementalResult::Buffered);
    require(recovery.on_incremental(35, 35, ready) == llab::FeedIncrementalResult::BufferFull);
    require(recovery.state() == llab::FeedRecoveryState::AwaitingSnapshot);
}
