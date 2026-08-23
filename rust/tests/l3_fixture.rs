use low_latency_lab_benchmarks::order_book::{Event, Level, OrderBook, Side};

fn side(text: &str) -> Side {
    match text {
        "B" => Side::Bid,
        "A" => Side::Ask,
        _ => panic!("invalid side"),
    }
}

#[test]
fn l3_smoke_fixture_matches_contract() {
    let trace = include_str!("../../data/fixtures/l3_smoke.llbt");
    let mut book = OrderBook::default();
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
        match fields[0] {
            "A" => book.apply(Event::Add {
                order_id: fields[1].parse().unwrap(),
                side: side(fields[2]),
                price_ticks: fields[3].parse().unwrap(),
                quantity: fields[4].parse().unwrap(),
            }),
            "C" => book.apply(Event::Cancel {
                order_id: fields[1].parse().unwrap(),
                quantity: fields[2].parse().unwrap(),
            }),
            "E" => book.apply(Event::Execute {
                order_id: fields[1].parse().unwrap(),
                quantity: fields[2].parse().unwrap(),
            }),
            "D" => book.apply(Event::Delete { order_id: fields[1].parse().unwrap() }),
            "R" => book.apply(Event::Replace {
                old_order_id: fields[1].parse().unwrap(),
                new_order_id: fields[2].parse().unwrap(),
                side: side(fields[3]),
                price_ticks: fields[4].parse().unwrap(),
                quantity: fields[5].parse().unwrap(),
            }),
            "U" => book.apply(Event::OrderUpsert {
                order_id: fields[1].parse().unwrap(),
                side: side(fields[2]),
                price_ticks: fields[3].parse().unwrap(),
                quantity: fields[4].parse().unwrap(),
            }),
            "X" => book.apply(Event::OrderDelete { order_id: fields[1].parse().unwrap() }),
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
                let expected_digest: u64 = fields[6].parse().unwrap();
                if expected_digest != 0 {
                    assert_eq!(book.state_digest(), expected_digest);
                }
                return;
            }
            other => panic!("invalid trace record: {other}"),
        }
        .unwrap();
    }
    panic!("fixture has no EXPECT record");
}
