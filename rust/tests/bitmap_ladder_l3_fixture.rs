use low_latency_lab_benchmarks::bitmap_ladder_order_book::OrderBook;
use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Level, Side};
fn side(s: &str) -> Side {
    match s {
        "B" => Side::Bid,
        "A" => Side::Ask,
        _ => panic!("side"),
    }
}
fn e(t: EventType, id: u64, id2: u64, s: Side, p: u64, q: u64) -> Event {
    Event {
        order_id: id,
        replacement_order_id: id2,
        price_ticks: p,
        quantity: q,
        event_type: t,
        side: s,
    }
}
#[test]
fn l3_smoke_fixture_matches_contract() {
    let mut book = OrderBook::new(16, 0, 256);
    for line in include_str!("../../data/fixtures/l3_smoke.llbt").lines() {
        if line.is_empty() || line.starts_with('#') || line == "LLBT/1" {
            continue;
        }
        let x: Vec<_> = line.split_ascii_whitespace().collect();
        match x[0] {
            "A" => book
                .apply(e(
                    EventType::Add,
                    x[1].parse().unwrap(),
                    0,
                    side(x[2]),
                    x[3].parse().unwrap(),
                    x[4].parse().unwrap(),
                ))
                .unwrap(),
            "C" | "E" => book
                .apply(e(
                    if x[0] == "C" { EventType::Cancel } else { EventType::Execute },
                    x[1].parse().unwrap(),
                    0,
                    Side::Bid,
                    0,
                    x[2].parse().unwrap(),
                ))
                .unwrap(),
            "D" | "X" => book
                .apply(e(
                    if x[0] == "D" { EventType::Delete } else { EventType::OrderDelete },
                    x[1].parse().unwrap(),
                    0,
                    Side::Bid,
                    0,
                    0,
                ))
                .unwrap(),
            "R" => book
                .apply(e(
                    EventType::Replace,
                    x[1].parse().unwrap(),
                    x[2].parse().unwrap(),
                    side(x[3]),
                    x[4].parse().unwrap(),
                    x[5].parse().unwrap(),
                ))
                .unwrap(),
            "U" => book
                .apply(e(
                    EventType::OrderUpsert,
                    x[1].parse().unwrap(),
                    0,
                    side(x[2]),
                    x[3].parse().unwrap(),
                    x[4].parse().unwrap(),
                ))
                .unwrap(),
            "EXPECT" => {
                assert_eq!(book.live_order_count(), x[1].parse::<usize>().unwrap());
                assert_eq!(
                    book.best_bid(),
                    Some(Level {
                        price_ticks: x[2].parse().unwrap(),
                        quantity: x[3].parse().unwrap()
                    })
                );
                assert_eq!(
                    book.best_ask(),
                    Some(Level {
                        price_ticks: x[4].parse().unwrap(),
                        quantity: x[5].parse().unwrap()
                    })
                );
                assert_eq!(book.state_digest(), x[6].parse::<u64>().unwrap());
                return;
            }
            _ => panic!("record"),
        }
    }
    panic!("no expect")
}
