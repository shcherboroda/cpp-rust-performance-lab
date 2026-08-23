use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Level, OrderBook, Side};

fn side(text: &str) -> Side {
    match text {
        "B" => Side::Bid,
        "A" => Side::Ask,
        _ => panic!("invalid side"),
    }
}

fn event(
    event_type: EventType,
    order_id: u64,
    replacement_order_id: u64,
    side: Side,
    price_ticks: u64,
    quantity: u64,
) -> Event {
    Event { order_id, replacement_order_id, price_ticks, quantity, event_type, side }
}

#[test]
fn l3_smoke_fixture_matches_contract() {
    assert_eq!(std::mem::size_of::<Event>(), 40);
    let trace = include_str!("../../data/fixtures/l3_smoke.llbt");
    let mut book = OrderBook::new(16);
    let mut header_seen = false;
    for line in trace.lines() {
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if !header_seen {
            assert_eq!(line, "LLBT/1");
            header_seen = true;
            continue;
        }
        let fields: Vec<_> = line.split_ascii_whitespace().collect();
        let applied = match fields[0] {
            "A" => book.apply(event(
                EventType::Add,
                fields[1].parse().unwrap(),
                0,
                side(fields[2]),
                fields[3].parse().unwrap(),
                fields[4].parse().unwrap(),
            )),
            "C" => book.apply(event(
                EventType::Cancel,
                fields[1].parse().unwrap(),
                0,
                Side::Bid,
                0,
                fields[2].parse().unwrap(),
            )),
            "E" => book.apply(event(
                EventType::Execute,
                fields[1].parse().unwrap(),
                0,
                Side::Bid,
                0,
                fields[2].parse().unwrap(),
            )),
            "D" => {
                book.apply(event(EventType::Delete, fields[1].parse().unwrap(), 0, Side::Bid, 0, 0))
            }
            "R" => book.apply(event(
                EventType::Replace,
                fields[1].parse().unwrap(),
                fields[2].parse().unwrap(),
                side(fields[3]),
                fields[4].parse().unwrap(),
                fields[5].parse().unwrap(),
            )),
            "U" => book.apply(event(
                EventType::OrderUpsert,
                fields[1].parse().unwrap(),
                0,
                side(fields[2]),
                fields[3].parse().unwrap(),
                fields[4].parse().unwrap(),
            )),
            "X" => book.apply(event(
                EventType::OrderDelete,
                fields[1].parse().unwrap(),
                0,
                Side::Bid,
                0,
                0,
            )),
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
            other => panic!("invalid trace record: {other}"),
        };
        applied.unwrap();
    }
    panic!("fixture has no EXPECT record");
}
