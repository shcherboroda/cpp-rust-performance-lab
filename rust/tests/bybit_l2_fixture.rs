use low_latency_lab_benchmarks::bybit_l2_order_book::{Change, Level, OrderBook, Side};
#[test]
fn snapshot_delta_and_restart_have_matching_semantics() {
    let mut b = OrderBook::new(50);
    assert!(b.apply_snapshot(10, &[Level { price: 10_000_000_000, quantity: 200_000_000 }, Level { price: 9_900_000_000, quantity: 100_000_000 }], &[Level { price: 10_100_000_000, quantity: 300_000_000 }, Level { price: 10_200_000_000, quantity: 100_000_000 }]));
    assert!(b.apply_delta(11, &[Change { side: Side::Bid, price: 10_000_000_000, quantity: 150_000_000 }, Change { side: Side::Bid, price: 9_975_000_000, quantity: 50_000_000 }, Change { side: Side::Ask, price: 10_100_000_000, quantity: 0 }]));
    assert!(b.apply_delta(12, &[Change { side: Side::Bid, price: 9_900_000_000, quantity: 0 }, Change { side: Side::Ask, price: 10_050_000_000, quantity: 250_000_000 }]));
    assert!(b.apply_snapshot(1, &[Level { price: 9_950_000_000, quantity: 400_000_000 }], &[Level { price: 10_050_000_000, quantity: 200_000_000 }]));
    assert!(b.apply_delta(2, &[Change { side: Side::Bid, price: 9_975_000_000, quantity: 100_000_000 }, Change { side: Side::Ask, price: 10_050_000_000, quantity: 0 }]));
    assert_eq!(b.best_bid(), Some(Level { price: 9_975_000_000, quantity: 100_000_000 })); assert_eq!(b.best_ask(), None); assert_eq!(b.level_count(), 2);
    assert!(!b.apply_delta(4, &[])); assert!(!b.valid());
    let mut bounded = OrderBook::new(1);
    assert!(bounded.apply_snapshot(1, &[Level { price: 1, quantity: 1 }], &[Level { price: 2, quantity: 1 }]));
    assert!(!bounded.apply_delta(2, &[Change { side: Side::Bid, price: 2, quantity: 1 }]));
    assert!(!bounded.valid());
}
