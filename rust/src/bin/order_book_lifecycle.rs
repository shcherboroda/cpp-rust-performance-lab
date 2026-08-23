use low_latency_lab_benchmarks::order_book::{Event, OrderBook, Side};
use std::fs::File;
use std::io::Write;
use std::time::Instant;

const ORDER_COUNT: usize = 32_768;
const WARMUP_SAMPLES: usize = 10;
const MEASURED_SAMPLES: usize = 200;

fn make_lifecycle_churn_trace() -> Vec<Event> {
    let mut events = Vec::with_capacity(ORDER_COUNT * 2);
    for index in 0..ORDER_COUNT {
        let is_bid = index % 2 == 0;
        let offset = (index % 128) as u64;
        let side = if is_bid { Side::Bid } else { Side::Ask };
        let price_ticks = if is_bid { 100_000 - offset } else { 100_001 + offset };
        let quantity = 1 + (index % 97) as u64;
        events.push(Event::Add { order_id: index as u64 + 1, side, price_ticks, quantity });
    }
    for index in 0..ORDER_COUNT {
        events
            .push(Event::Execute { order_id: index as u64 + 1, quantity: 1 + (index % 97) as u64 });
    }
    events
}

fn run_sample(events: &[Event]) -> u128 {
    let mut book = OrderBook::with_order_capacity(ORDER_COUNT);
    let start = Instant::now();
    for &event in events {
        book.apply(event).expect("generated lifecycle trace must be valid");
    }
    let elapsed = start.elapsed().as_nanos();
    assert_eq!(book.live_order_count(), 0, "lifecycle trace did not restore an empty order book");
    assert_eq!(book.best_bid(), None, "lifecycle trace left a bid level");
    assert_eq!(book.best_ask(), None, "lifecycle trace left an ask level");
    elapsed
}

fn nearest_rank_index(count: usize, percentile: f64) -> usize {
    (percentile * count as f64).ceil() as usize - 1
}

fn main() {
    let args: Vec<_> = std::env::args().collect();
    let raw_path = match args.as_slice() {
        [_] => None,
        [_, flag, path] if flag == "--raw" => Some(path),
        _ => panic!("usage: order_book_lifecycle [--raw PATH]"),
    };

    let events = make_lifecycle_churn_trace();
    let layout_book = OrderBook::with_order_capacity(ORDER_COUNT);
    for _ in 0..WARMUP_SAMPLES {
        let _ = run_sample(&events);
    }
    let mut samples = Vec::with_capacity(MEASURED_SAMPLES);
    for _ in 0..MEASURED_SAMPLES {
        samples.push(run_sample(&events));
    }
    if let Some(path) = raw_path {
        let mut raw = File::create(path).expect("cannot write raw sample file");
        writeln!(raw, "sample_index,duration_ns").unwrap();
        for (index, duration_ns) in samples.iter().enumerate() {
            writeln!(raw, "{index},{duration_ns}").unwrap();
        }
    }

    let mut sorted = samples.clone();
    sorted.sort_unstable();
    let mean_ns = samples.iter().sum::<u128>() as f64 / samples.len() as f64;
    let p50_ns = sorted[nearest_rank_index(sorted.len(), 0.50)];
    let events_per_second = events.len() as f64 * 1_000_000_000.0 / p50_ns as f64;

    println!("format: llab_benchmark_v1");
    println!("benchmark: order_book_lifecycle_churn_v1");
    println!("language: rust");
    println!("event_size_bytes: {}", std::mem::size_of::<Event>());
    println!("order_book_size_bytes: {}", std::mem::size_of::<OrderBook>());
    println!("order_index_capacity: {}", layout_book.order_index_capacity());
    println!("orders_per_sample: {ORDER_COUNT}");
    println!("events_per_sample: {}", events.len());
    println!("warmup_samples: {WARMUP_SAMPLES}");
    println!("measured_samples: {MEASURED_SAMPLES}");
    println!("min_ns: {}", sorted[0]);
    println!("p50_ns: {}", sorted[nearest_rank_index(sorted.len(), 0.50)]);
    println!("p90_ns: {}", sorted[nearest_rank_index(sorted.len(), 0.90)]);
    println!("p95_ns: {}", sorted[nearest_rank_index(sorted.len(), 0.95)]);
    println!("p99_ns: {}", sorted[nearest_rank_index(sorted.len(), 0.99)]);
    println!("p999_ns: {}", sorted[nearest_rank_index(sorted.len(), 0.999)]);
    println!("max_ns: {}", sorted[sorted.len() - 1]);
    println!("mean_ns: {mean_ns:.2}");
    println!("p50_ns_per_event: {:.2}", p50_ns as f64 / events.len() as f64);
    println!("p50_events_per_s: {events_per_second:.2}");
}
