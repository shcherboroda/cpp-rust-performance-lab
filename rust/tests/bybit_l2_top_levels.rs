use low_latency_lab_benchmarks::bybit_l2_order_book::{Level, OrderBook, Side};

#[test]
fn exposes_sorted_top_levels_only_while_valid() {
    let mut book = OrderBook::new(10);
    assert!(book.apply_snapshot(1, &[Level { price: 100, quantity: 2 }, Level { price: 99, quantity: 1 }], &[Level { price: 101, quantity: 3 }]));
    assert_eq!(book.top_levels(Side::Bid, 10), &[Level { price: 100, quantity: 2 }, Level { price: 99, quantity: 1 }]);
    assert!(!book.apply_delta(3, &[]));
    assert!(book.top_levels(Side::Bid, 10).is_empty());
}
