use low_latency_lab_benchmarks::dense_ladder_order_book::OrderBook;
use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Side};
use std::fs::File;
use std::io::Write;
use std::time::Instant;

const ORDER_COUNT: usize = 32_768;
const WARMUP_SAMPLES: usize = 10;
const MEASURED_SAMPLES: usize = 200;

fn make_best_level_churn_trace() -> Vec<Event> {
    let mut events = Vec::with_capacity(ORDER_COUNT * 2);
    for index in 0..ORDER_COUNT {
        let side = if index % 2 == 0 { Side::Bid } else { Side::Ask };
        let price_ticks = if side == Side::Bid { 100_128 } else { 99_873 };
        let quantity = 1 + index as u64 % 97;
        events.push(Event {
            order_id: index as u64 + 1,
            replacement_order_id: 0,
            price_ticks,
            quantity,
            event_type: EventType::Add,
            side,
        });
        events.push(Event {
            order_id: index as u64 + 1,
            replacement_order_id: 0,
            price_ticks,
            quantity,
            event_type: EventType::Execute,
            side,
        });
    }
    events
}

fn bbo_digest(book: &OrderBook) -> u64 {
    let mut digest = 0x9E37_79B9_7F4A_7C15;
    if let Some(bid) = book.best_bid() {
        digest ^= bid.price_ticks ^ (bid.quantity << 1);
    }
    if let Some(ask) = book.best_ask() {
        digest ^= (ask.price_ticks << 7) ^ (ask.quantity << 3);
    }
    digest
}

fn run_sample(events: &[Event], digest: &mut u64) -> u128 {
    let mut book = OrderBook::new(ORDER_COUNT, 99_873, 256);
    let start = Instant::now();
    for &event in events {
        book.apply(event).expect("generated trace must be valid");
        *digest ^= bbo_digest(&book)
            .wrapping_add(0x9E37_79B9_7F4A_7C15)
            .wrapping_add(*digest << 6)
            .wrapping_add(*digest >> 2);
    }
    let elapsed = start.elapsed().as_nanos();
    assert_eq!(book.live_order_count(), 0);
    assert_eq!(book.best_bid(), None);
    assert_eq!(book.best_ask(), None);
    elapsed
}

fn rank(n: usize, p: f64) -> usize {
    (n as f64 * p).ceil() as usize - 1
}

fn main() {
    let args: Vec<_> = std::env::args().collect();
    let raw_path = match args.as_slice() {
        [_] => None,
        [_, flag, path] if flag == "--raw" => Some(path),
        _ => panic!("usage: dense_ladder_best_level_churn [--raw PATH]"),
    };
    let events = make_best_level_churn_trace();
    let layout_book = OrderBook::new(ORDER_COUNT, 99_873, 256);
    let mut digest = 0;
    for _ in 0..WARMUP_SAMPLES {
        let _ = run_sample(&events, &mut digest);
    }
    let mut samples = Vec::with_capacity(MEASURED_SAMPLES);
    for _ in 0..MEASURED_SAMPLES {
        samples.push(run_sample(&events, &mut digest));
    }
    if let Some(path) = raw_path {
        let mut raw = File::create(path).expect("cannot write raw");
        writeln!(raw, "sample_index,duration_ns").unwrap();
        for (index, duration_ns) in samples.iter().enumerate() {
            writeln!(raw, "{index},{duration_ns}").unwrap();
        }
    }
    let mut sorted = samples.clone();
    sorted.sort_unstable();
    let p50 = sorted[rank(sorted.len(), 0.50)];
    let mean = samples.iter().sum::<u128>() as f64 / samples.len() as f64;
    println!(
        "format: llab_benchmark_v1\nbenchmark: dense_ladder_best_level_churn_v3\nlanguage: rust"
    );
    println!(
        "event_size_bytes: {}\norder_book_size_bytes: {}\norder_index_capacity: {}",
        std::mem::size_of::<Event>(),
        std::mem::size_of::<OrderBook>(),
        layout_book.order_index_capacity()
    );
    println!(
        "orders_per_sample: {ORDER_COUNT}\nevents_per_sample: {}\nwarmup_samples: {WARMUP_SAMPLES}\nmeasured_samples: {MEASURED_SAMPLES}",
        events.len()
    );
    println!(
        "min_ns: {}\np50_ns: {p50}\np90_ns: {}\np95_ns: {}\np99_ns: {}\np999_ns: {}\nmax_ns: {}\nmean_ns: {mean:.2}\np50_ns_per_event: {:.2}\np50_events_per_s: {:.2}\nresult_digest: {digest}",
        sorted[0],
        sorted[rank(sorted.len(), 0.90)],
        sorted[rank(sorted.len(), 0.95)],
        sorted[rank(sorted.len(), 0.99)],
        sorted[rank(sorted.len(), 0.999)],
        sorted[sorted.len() - 1],
        p50 as f64 / events.len() as f64,
        events.len() as f64 * 1_000_000_000.0 / p50 as f64
    );
}
