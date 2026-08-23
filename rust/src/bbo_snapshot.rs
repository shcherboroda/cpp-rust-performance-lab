use std::sync::atomic::{AtomicU64, Ordering};

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct BboSnapshot {
    pub bid_price: u64,
    pub bid_quantity: u64,
    pub ask_price: u64,
    pub ask_quantity: u64,
}

/// Single-writer, multi-reader seqlock BBO publication cell.
///
/// `try_read` returns an internally consistent snapshot or `None` when a
/// writer overlaps the read. The caller must serialize writers.
#[repr(align(64))]
pub struct BboSnapshotCell {
    version: AtomicU64,
    bid_price: AtomicU64,
    bid_quantity: AtomicU64,
    ask_price: AtomicU64,
    ask_quantity: AtomicU64,
}

/// Same publication protocol without cache-line alignment.  It exists only as
/// a controlled benchmark baseline; production code should use
/// `BboSnapshotCell` to avoid placing unrelated state in the writer's line.
pub struct UnalignedBboSnapshotCell {
    version: AtomicU64,
    bid_price: AtomicU64,
    bid_quantity: AtomicU64,
    ask_price: AtomicU64,
    ask_quantity: AtomicU64,
}

impl BboSnapshotCell {
    pub const fn new() -> Self {
        Self {
            version: AtomicU64::new(0),
            bid_price: AtomicU64::new(0),
            bid_quantity: AtomicU64::new(0),
            ask_price: AtomicU64::new(0),
            ask_quantity: AtomicU64::new(0),
        }
    }

    pub fn publish(&self, snapshot: BboSnapshot) {
        let starting_version = self.version.fetch_add(1, Ordering::AcqRel);
        self.bid_price.store(snapshot.bid_price, Ordering::Relaxed);
        self.bid_quantity.store(snapshot.bid_quantity, Ordering::Relaxed);
        self.ask_price.store(snapshot.ask_price, Ordering::Relaxed);
        self.ask_quantity.store(snapshot.ask_quantity, Ordering::Relaxed);
        self.version.store(starting_version + 2, Ordering::Release);
    }

    pub fn try_read(&self) -> Option<BboSnapshot> {
        let before = self.version.load(Ordering::Acquire);
        if before & 1 != 0 {
            return None;
        }
        let snapshot = BboSnapshot {
            bid_price: self.bid_price.load(Ordering::Relaxed),
            bid_quantity: self.bid_quantity.load(Ordering::Relaxed),
            ask_price: self.ask_price.load(Ordering::Relaxed),
            ask_quantity: self.ask_quantity.load(Ordering::Relaxed),
        };
        let after = self.version.load(Ordering::Acquire);
        (before == after).then_some(snapshot)
    }
}

impl Default for BboSnapshotCell {
    fn default() -> Self {
        Self::new()
    }
}

impl UnalignedBboSnapshotCell {
    pub const fn new() -> Self {
        Self {
            version: AtomicU64::new(0),
            bid_price: AtomicU64::new(0),
            bid_quantity: AtomicU64::new(0),
            ask_price: AtomicU64::new(0),
            ask_quantity: AtomicU64::new(0),
        }
    }

    pub fn publish(&self, snapshot: BboSnapshot) {
        let starting_version = self.version.fetch_add(1, Ordering::AcqRel);
        self.bid_price.store(snapshot.bid_price, Ordering::Relaxed);
        self.bid_quantity.store(snapshot.bid_quantity, Ordering::Relaxed);
        self.ask_price.store(snapshot.ask_price, Ordering::Relaxed);
        self.ask_quantity.store(snapshot.ask_quantity, Ordering::Relaxed);
        self.version.store(starting_version + 2, Ordering::Release);
    }

    pub fn try_read(&self) -> Option<BboSnapshot> {
        let before = self.version.load(Ordering::Acquire);
        if before & 1 != 0 {
            return None;
        }
        let snapshot = BboSnapshot {
            bid_price: self.bid_price.load(Ordering::Relaxed),
            bid_quantity: self.bid_quantity.load(Ordering::Relaxed),
            ask_price: self.ask_price.load(Ordering::Relaxed),
            ask_quantity: self.ask_quantity.load(Ordering::Relaxed),
        };
        let after = self.version.load(Ordering::Acquire);
        (before == after).then_some(snapshot)
    }
}

impl Default for UnalignedBboSnapshotCell {
    fn default() -> Self {
        Self::new()
    }
}
