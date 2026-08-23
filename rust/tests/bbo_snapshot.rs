use low_latency_lab_benchmarks::bbo_snapshot::{BboSnapshot, BboSnapshotCell};
use std::sync::{
    Arc,
    atomic::{AtomicBool, Ordering},
};
use std::thread;

#[test]
fn is_cache_line_aligned() {
    assert_eq!(std::mem::align_of::<BboSnapshotCell>(), 64);
    assert_eq!(std::mem::size_of::<BboSnapshotCell>(), 64);
}

#[test]
fn publishes_a_consistent_snapshot() {
    let cell = BboSnapshotCell::new();
    assert_eq!(cell.try_read(), Some(BboSnapshot::default()));
    let snapshot =
        BboSnapshot { bid_price: 100, bid_quantity: 20, ask_price: 101, ask_quantity: 30 };
    cell.publish(snapshot);
    assert_eq!(cell.try_read(), Some(snapshot));
}

#[test]
fn concurrent_reader_never_accepts_a_torn_snapshot() {
    let cell = Arc::new(BboSnapshotCell::new());
    cell.publish(BboSnapshot { bid_price: 1, bid_quantity: 2, ask_price: 3, ask_quantity: 4 });
    let writer_cell = Arc::clone(&cell);
    let done = Arc::new(AtomicBool::new(false));
    let writer_done = Arc::clone(&done);
    let writer = thread::spawn(move || {
        for value in 1..=100_000 {
            writer_cell.publish(BboSnapshot {
                bid_price: value,
                bid_quantity: value + 1,
                ask_price: value + 2,
                ask_quantity: value + 3,
            });
        }
        writer_done.store(true, Ordering::Release);
    });
    while !done.load(Ordering::Acquire) {
        if let Some(snapshot) = cell.try_read() {
            assert_eq!(snapshot.bid_quantity, snapshot.bid_price + 1);
            assert_eq!(snapshot.ask_price, snapshot.bid_price + 2);
            assert_eq!(snapshot.ask_quantity, snapshot.bid_price + 3);
        }
    }
    writer.join().unwrap();
}
