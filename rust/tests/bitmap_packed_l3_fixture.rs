use low_latency_lab_benchmarks::bitmap_packed_order_book::OrderBook;
use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Level, Side};

fn side(text: &str) -> Side {
    match text {
        "B" => Side::Bid,
        "A" => Side::Ask,
        _ => panic!("side"),
    }
}

fn event(
    kind: EventType,
    id: u64,
    replacement_id: u64,
    side: Side,
    price: u64,
    quantity: u64,
) -> Event {
    Event {
        order_id: id,
        replacement_order_id: replacement_id,
        price_ticks: price,
        quantity,
        event_type: kind,
        side,
    }
}

#[test]
fn l3_smoke_fixture_matches_contract() {
    let mut book = OrderBook::new(16, 0, 256);
    for line in include_str!("../../data/fixtures/l3_smoke.llbt").lines() {
        if line.is_empty() || line.starts_with('#') || line == "LLBT/1" {
            continue;
        }
        let fields: Vec<_> = line.split_ascii_whitespace().collect();
        match fields[0] {
            "A" => book
                .apply(event(
                    EventType::Add,
                    fields[1].parse().unwrap(),
                    0,
                    side(fields[2]),
                    fields[3].parse().unwrap(),
                    fields[4].parse().unwrap(),
                ))
                .unwrap(),
            "C" | "E" => book
                .apply(event(
                    if fields[0] == "C" { EventType::Cancel } else { EventType::Execute },
                    fields[1].parse().unwrap(),
                    0,
                    Side::Bid,
                    0,
                    fields[2].parse().unwrap(),
                ))
                .unwrap(),
            "D" | "X" => book
                .apply(event(
                    if fields[0] == "D" { EventType::Delete } else { EventType::OrderDelete },
                    fields[1].parse().unwrap(),
                    0,
                    Side::Bid,
                    0,
                    0,
                ))
                .unwrap(),
            "R" => book
                .apply(event(
                    EventType::Replace,
                    fields[1].parse().unwrap(),
                    fields[2].parse().unwrap(),
                    side(fields[3]),
                    fields[4].parse().unwrap(),
                    fields[5].parse().unwrap(),
                ))
                .unwrap(),
            "U" => book
                .apply(event(
                    EventType::OrderUpsert,
                    fields[1].parse().unwrap(),
                    0,
                    side(fields[2]),
                    fields[3].parse().unwrap(),
                    fields[4].parse().unwrap(),
                ))
                .unwrap(),
            "EXPECT" => {
                assert_eq!(book.live_order_count(), fields[1].parse::<usize>().unwrap());
                assert_eq!(
                    book.best_bid(),
                    Some(Level {
                        price_ticks: fields[2].parse().unwrap(),
                        quantity: fields[3].parse().unwrap()
                    })
                );
                assert_eq!(
                    book.best_ask(),
                    Some(Level {
                        price_ticks: fields[4].parse().unwrap(),
                        quantity: fields[5].parse().unwrap()
                    })
                );
                assert_eq!(book.state_digest(), fields[6].parse::<u64>().unwrap());
                return;
            }
            _ => panic!("record"),
        }
    }
    panic!("no EXPECT record");
}
