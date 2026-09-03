const BUCKETS: usize = 64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FrameClass {
    Control,
    Snapshot,
    MarketUpdate,
}

#[derive(Debug)]
pub struct FeedReceiveMetrics {
    control_frames: u64,
    snapshots: u64,
    market_updates: u64,
    queue_overflows: u64,
    oversized_frames: u64,
    previous_market_update_ns: Option<u64>,
    interarrival: [u64; BUCKETS],
    callback_to_handoff: [u64; BUCKETS],
}

impl Default for FeedReceiveMetrics {
    fn default() -> Self {
        Self {
            control_frames: 0,
            snapshots: 0,
            market_updates: 0,
            queue_overflows: 0,
            oversized_frames: 0,
            previous_market_update_ns: None,
            interarrival: [0; BUCKETS],
            callback_to_handoff: [0; BUCKETS],
        }
    }
}

impl FeedReceiveMetrics {
    pub fn record_frame(
        &mut self,
        class: FrameClass,
        received_ns: u64,
        callback_to_handoff_ns: u64,
    ) {
        self.callback_to_handoff[bucket(callback_to_handoff_ns)] += 1;
        match class {
            FrameClass::Control => self.control_frames += 1,
            FrameClass::Snapshot => self.snapshots += 1,
            FrameClass::MarketUpdate => {
                if let Some(previous) = self.previous_market_update_ns {
                    self.interarrival[bucket(received_ns.saturating_sub(previous))] += 1;
                }
                self.previous_market_update_ns = Some(received_ns);
                self.market_updates += 1;
            }
        }
    }

    pub fn record_queue_overflow(&mut self) {
        self.queue_overflows += 1;
    }
    pub fn record_oversized_frame(&mut self) {
        self.oversized_frames += 1;
    }
    pub fn snapshot(&self) -> FeedReceiveSnapshot {
        FeedReceiveSnapshot {
            control_frames: self.control_frames,
            snapshots: self.snapshots,
            market_updates: self.market_updates,
            queue_overflows: self.queue_overflows,
            oversized_frames: self.oversized_frames,
            interarrival: self.interarrival,
            callback_to_handoff: self.callback_to_handoff,
        }
    }
}

#[derive(Debug, Eq, PartialEq)]
pub struct FeedReceiveSnapshot {
    pub control_frames: u64,
    pub snapshots: u64,
    pub market_updates: u64,
    pub queue_overflows: u64,
    pub oversized_frames: u64,
    pub interarrival: [u64; BUCKETS],
    pub callback_to_handoff: [u64; BUCKETS],
}

fn bucket(ns: u64) -> usize {
    if ns == 0 { 0 } else { (63 - ns.leading_zeros() as usize).min(BUCKETS - 1) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn excludes_control_and_snapshot_from_market_interarrival() {
        let mut metrics = FeedReceiveMetrics::default();
        metrics.record_frame(FrameClass::Control, 10, 3);
        metrics.record_frame(FrameClass::Snapshot, 20, 5);
        metrics.record_frame(FrameClass::MarketUpdate, 100, 7);
        metrics.record_frame(FrameClass::MarketUpdate, 132, 9);
        metrics.record_queue_overflow();
        let snapshot = metrics.snapshot();
        assert_eq!(snapshot.control_frames, 1);
        assert_eq!(snapshot.snapshots, 1);
        assert_eq!(snapshot.market_updates, 2);
        assert_eq!(snapshot.queue_overflows, 1);
        assert_eq!(snapshot.interarrival[bucket(32)], 1);
        assert_eq!(snapshot.callback_to_handoff[bucket(7)], 2);
        assert_eq!(snapshot.callback_to_handoff[bucket(9)], 1);
    }
}
