use low_latency_lab_benchmarks::bybit_l2_order_book::{Change, Level, Side};
use low_latency_lab_benchmarks::bybit_l2_dense_ladder::DenseLadderBook;
use low_latency_lab_benchmarks::bybit_l2_ordered_map::OrderedMapBook;
#[test] fn dense_and_tree_match_vector_semantics() {
 let bids=[Level{price:100,quantity:2},Level{price:99,quantity:1}];let asks=[Level{price:101,quantity:3}];let d=[Change{side:Side::Bid,price:100,quantity:1},Change{side:Side::Ask,price:101,quantity:0},Change{side:Side::Ask,price:102,quantity:4}];
 let mut dense=DenseLadderBook::new(90,20);let mut tree=OrderedMapBook::new();assert!(dense.apply_snapshot(1,&bids,&asks)&&tree.apply_snapshot(1,&bids,&asks));assert!(dense.apply_delta(2,&d)&&tree.apply_delta(2,&d));assert_eq!(dense.best_bid(),tree.best_bid());assert_eq!(dense.best_ask(),tree.best_ask());
}
