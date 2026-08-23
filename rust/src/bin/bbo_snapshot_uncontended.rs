use low_latency_lab_benchmarks::bbo_snapshot::{
    BboSnapshot, BboSnapshotCell, UnalignedBboSnapshotCell,
};
use std::time::Instant;

trait SnapshotCell {
    fn new() -> Self;
    fn publish(&self, snapshot: BboSnapshot);
    fn try_read(&self) -> Option<BboSnapshot>;
}
impl SnapshotCell for BboSnapshotCell {
    fn new() -> Self {
        Self::new()
    }
    fn publish(&self, s: BboSnapshot) {
        self.publish(s)
    }
    fn try_read(&self) -> Option<BboSnapshot> {
        self.try_read()
    }
}
impl SnapshotCell for UnalignedBboSnapshotCell {
    fn new() -> Self {
        Self::new()
    }
    fn publish(&self, s: BboSnapshot) {
        self.publish(s)
    }
    fn try_read(&self) -> Option<BboSnapshot> {
        self.try_read()
    }
}

fn sample<C: SnapshotCell>() -> u128 {
    const OPERATIONS: u64 = 100_000;
    let cell = C::new();
    let mut digest = 0;
    let start = Instant::now();
    for value in 1..=OPERATIONS {
        cell.publish(BboSnapshot {
            bid_price: value,
            bid_quantity: value + 1,
            ask_price: value + 2,
            ask_quantity: value + 3,
        });
        digest ^= cell.try_read().unwrap().bid_price;
    }
    std::hint::black_box(digest);
    start.elapsed().as_nanos()
}

fn report<C: SnapshotCell>(name: &str) {
    for _ in 0..10 {
        let _ = sample::<C>();
    }
    let mut samples: Vec<_> = (0..200).map(|_| sample::<C>()).collect();
    samples.sort_unstable();
    println!("{name} operations_per_sample=100000 p50_ns={} p99_ns={}", samples[99], samples[197]);
}

fn main() {
    if std::env::args().nth(1).as_deref() == Some("--reverse") {
        report::<BboSnapshotCell>("aligned");
        report::<UnalignedBboSnapshotCell>("unaligned");
    } else {
        report::<UnalignedBboSnapshotCell>("unaligned");
        report::<BboSnapshotCell>("aligned");
    }
}
