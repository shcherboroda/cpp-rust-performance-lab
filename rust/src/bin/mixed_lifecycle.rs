use low_latency_lab_benchmarks::bitmap_backshift_order_book;
use low_latency_lab_benchmarks::bitmap_ladder_order_book;
use low_latency_lab_benchmarks::bitmap_packed_order_book;
use low_latency_lab_benchmarks::dense_ladder_order_book;
use low_latency_lab_benchmarks::order_book;
use low_latency_lab_benchmarks::parity_order_book;
use std::{fs::File, io::Write, time::Instant};

const CYCLES: usize = 8_192;
const MAXIMUM_LIVE_ORDERS: usize = 32_768;
const WARMUP_SAMPLES: usize = 10;
const MEASURED_SAMPLES: usize = 200;

#[derive(Clone, Copy)]
struct LogicalEvent {
    kind: parity_order_book::EventType,
    order_id: u64,
    replacement_order_id: u64,
    side: parity_order_book::Side,
    price_ticks: u64,
    quantity: u64,
}

fn trace() -> Vec<LogicalEvent> {
    let mut events = Vec::with_capacity(CYCLES * 7);
    for cycle in 0..CYCLES {
        let a = cycle as u64 * 3 + 1;
        let b = a + 1;
        let c = a + 2;
        let offset = cycle as u64 % 128;
        let a_side = if cycle % 2 == 0 {
            parity_order_book::Side::Bid
        } else {
            parity_order_book::Side::Ask
        };
        let b_side = if a_side == parity_order_book::Side::Bid {
            parity_order_book::Side::Ask
        } else {
            parity_order_book::Side::Bid
        };
        let price = |side| {
            if side == parity_order_book::Side::Bid { 100_000 - offset } else { 100_001 + offset }
        };
        events.extend([
            LogicalEvent {
                kind: parity_order_book::EventType::Add,
                order_id: a,
                replacement_order_id: 0,
                side: a_side,
                price_ticks: price(a_side),
                quantity: 10,
            },
            LogicalEvent {
                kind: parity_order_book::EventType::Add,
                order_id: b,
                replacement_order_id: 0,
                side: b_side,
                price_ticks: price(b_side),
                quantity: 20,
            },
            LogicalEvent {
                kind: parity_order_book::EventType::Cancel,
                order_id: a,
                replacement_order_id: 0,
                side: a_side,
                price_ticks: 0,
                quantity: 3,
            },
            LogicalEvent {
                kind: parity_order_book::EventType::OrderUpsert,
                order_id: b,
                replacement_order_id: 0,
                side: b_side,
                price_ticks: price(b_side),
                quantity: 15,
            },
            LogicalEvent {
                kind: parity_order_book::EventType::Replace,
                order_id: a,
                replacement_order_id: c,
                side: a_side,
                price_ticks: price(a_side),
                quantity: 7,
            },
            LogicalEvent {
                kind: parity_order_book::EventType::Execute,
                order_id: c,
                replacement_order_id: 0,
                side: a_side,
                price_ticks: 0,
                quantity: 7,
            },
            LogicalEvent {
                kind: if cycle % 2 == 0 {
                    parity_order_book::EventType::Delete
                } else {
                    parity_order_book::EventType::OrderDelete
                },
                order_id: b,
                replacement_order_id: 0,
                side: b_side,
                price_ticks: 0,
                quantity: 0,
            },
        ]);
    }
    events
}

fn run_native(events: &[LogicalEvent], read_bbo: bool, digest: &mut u64) -> u128 {
    let mut book = order_book::OrderBook::with_order_capacity(MAXIMUM_LIVE_ORDERS);
    let start = Instant::now();
    for e in events {
        use order_book::{Event, Side};
        let side = if e.side == parity_order_book::Side::Bid { Side::Bid } else { Side::Ask };
        let event = match e.kind {
            parity_order_book::EventType::Add => Event::Add {
                order_id: e.order_id,
                side,
                price_ticks: e.price_ticks,
                quantity: e.quantity,
            },
            parity_order_book::EventType::Cancel => {
                Event::Cancel { order_id: e.order_id, quantity: e.quantity }
            }
            parity_order_book::EventType::Execute => {
                Event::Execute { order_id: e.order_id, quantity: e.quantity }
            }
            parity_order_book::EventType::Delete => Event::Delete { order_id: e.order_id },
            parity_order_book::EventType::Replace => Event::Replace {
                old_order_id: e.order_id,
                new_order_id: e.replacement_order_id,
                side,
                price_ticks: e.price_ticks,
                quantity: e.quantity,
            },
            parity_order_book::EventType::OrderUpsert => Event::OrderUpsert {
                order_id: e.order_id,
                side,
                price_ticks: e.price_ticks,
                quantity: e.quantity,
            },
            parity_order_book::EventType::OrderDelete => {
                Event::OrderDelete { order_id: e.order_id }
            }
        };
        book.apply(event).unwrap();
        if read_bbo {
            let mut value = 0x9E37_79B9_7F4A_7C15;
            if let Some(level) = book.best_bid() {
                value ^= level.price_ticks ^ (level.quantity << 1);
            }
            if let Some(level) = book.best_ask() {
                value ^= (level.price_ticks << 7) ^ (level.quantity << 3);
            }
            *digest ^= value
                .wrapping_add(0x9E37_79B9_7F4A_7C15)
                .wrapping_add(*digest << 6)
                .wrapping_add(*digest >> 2);
        }
    }
    let elapsed = start.elapsed().as_nanos();
    assert_eq!(book.live_order_count(), 0);
    assert_eq!(book.best_bid(), None);
    assert_eq!(book.best_ask(), None);
    elapsed
}

fn run_parity(
    events: &[LogicalEvent],
    dense: bool,
    bitmap: bool,
    backshift: bool,
    packed: bool,
    read_bbo: bool,
    digest: &mut u64,
) -> u128 {
    if dense {
        let mut book = dense_ladder_order_book::OrderBook::new(MAXIMUM_LIVE_ORDERS, 99_873, 256);
        let start = Instant::now();
        for &e in events {
            book.apply(parity_event(e)).unwrap();
            if read_bbo {
                *digest ^=
                    fold_parity_bbo(book.best_bid(), book.best_ask()).wrapping_add(*digest << 1);
            }
        }
        assert_eq!(book.live_order_count(), 0);
        assert_eq!(book.best_bid(), None);
        assert_eq!(book.best_ask(), None);
        return start.elapsed().as_nanos();
    } else if bitmap {
        let mut book = bitmap_ladder_order_book::OrderBook::new(MAXIMUM_LIVE_ORDERS, 99_873, 256);
        let start = Instant::now();
        for &e in events {
            book.apply(parity_event(e)).unwrap();
            if read_bbo {
                *digest ^=
                    fold_parity_bbo(book.best_bid(), book.best_ask()).wrapping_add(*digest << 1);
            }
        }
        assert_eq!(book.live_order_count(), 0);
        assert_eq!(book.best_bid(), None);
        assert_eq!(book.best_ask(), None);
        return start.elapsed().as_nanos();
    } else if backshift {
        let mut book =
            bitmap_backshift_order_book::OrderBook::new(MAXIMUM_LIVE_ORDERS, 99_873, 256);
        let start = Instant::now();
        for &e in events {
            book.apply(parity_event(e)).unwrap();
            if read_bbo {
                *digest ^=
                    fold_parity_bbo(book.best_bid(), book.best_ask()).wrapping_add(*digest << 1);
            }
        }
        assert_eq!(book.live_order_count(), 0);
        assert_eq!(book.best_bid(), None);
        assert_eq!(book.best_ask(), None);
        return start.elapsed().as_nanos();
    } else if packed {
        let mut book = bitmap_packed_order_book::OrderBook::new(MAXIMUM_LIVE_ORDERS, 99_873, 256);
        let start = Instant::now();
        for &e in events {
            book.apply(parity_event(e)).unwrap();
            if read_bbo {
                *digest ^=
                    fold_parity_bbo(book.best_bid(), book.best_ask()).wrapping_add(*digest << 1);
            }
        }
        assert_eq!(book.live_order_count(), 0);
        assert_eq!(book.best_bid(), None);
        assert_eq!(book.best_ask(), None);
        return start.elapsed().as_nanos();
    } else {
        let mut book = parity_order_book::OrderBook::new(MAXIMUM_LIVE_ORDERS);
        let start = Instant::now();
        for &e in events {
            book.apply(parity_event(e)).unwrap();
            if read_bbo {
                *digest ^=
                    fold_parity_bbo(book.best_bid(), book.best_ask()).wrapping_add(*digest << 1);
            }
        }
        assert_eq!(book.live_order_count(), 0);
        assert_eq!(book.best_bid(), None);
        assert_eq!(book.best_ask(), None);
        return start.elapsed().as_nanos();
    }
}

fn fold_parity_bbo(
    bid: Option<parity_order_book::Level>,
    ask: Option<parity_order_book::Level>,
) -> u64 {
    let mut value = 0x9E37_79B9_7F4A_7C15;
    if let Some(level) = bid {
        value ^= level.price_ticks ^ (level.quantity << 1);
    }
    if let Some(level) = ask {
        value ^= (level.price_ticks << 7) ^ (level.quantity << 3);
    }
    value
}

fn parity_event(e: LogicalEvent) -> parity_order_book::Event {
    parity_order_book::Event {
        order_id: e.order_id,
        replacement_order_id: e.replacement_order_id,
        price_ticks: e.price_ticks,
        quantity: e.quantity,
        event_type: e.kind,
        side: e.side,
    }
}

pub fn main() {
    let name = option_env!("CARGO_BIN_NAME").unwrap();
    let raw_path = match std::env::args().collect::<Vec<_>>().as_slice() {
        [_] => None,
        [_, flag, path] if flag == "--raw" => Some(path.clone()),
        _ => panic!("usage: mixed_lifecycle [--raw PATH]"),
    };
    let read_bbo = name.ends_with("_with_bbo");
    let variant = name.trim_end_matches("_with_bbo");
    let events = trace();
    let mut digest = 0;
    let mut run = || match variant {
        "native_mixed_lifecycle" => run_native(&events, read_bbo, &mut digest),
        "parity_mixed_lifecycle" => {
            run_parity(&events, false, false, false, false, read_bbo, &mut digest)
        }
        "dense_mixed_lifecycle" => {
            run_parity(&events, true, false, false, false, read_bbo, &mut digest)
        }
        "bitmap_mixed_lifecycle" => {
            run_parity(&events, false, true, false, false, read_bbo, &mut digest)
        }
        "bitmap_backshift_mixed_lifecycle" => {
            run_parity(&events, false, false, true, false, read_bbo, &mut digest)
        }
        "bitmap_packed_mixed_lifecycle" => {
            run_parity(&events, false, false, false, true, read_bbo, &mut digest)
        }
        _ => panic!("unknown binary"),
    };
    for _ in 0..WARMUP_SAMPLES {
        let _ = run();
    }
    let mut samples = Vec::with_capacity(MEASURED_SAMPLES);
    for _ in 0..MEASURED_SAMPLES {
        samples.push(run());
    }
    if let Some(path) = raw_path {
        let mut raw = File::create(path).expect("cannot write raw samples");
        writeln!(raw, "sample_index,duration_ns").unwrap();
        for (index, sample) in samples.iter().enumerate() {
            writeln!(raw, "{index},{sample}").unwrap();
        }
    }
    samples.sort_unstable();
    println!(
        "format: llab_benchmark_v1\nbenchmark: stream_mixed_lifecycle_{name}_v1\nlanguage: rust\nevents_per_sample: {}\nwarmup_samples: {WARMUP_SAMPLES}\nmeasured_samples: {MEASURED_SAMPLES}\np50_ns: {}\np99_ns: {}\nresult_digest: {digest}",
        events.len(),
        samples[99],
        samples[197]
    );
}
