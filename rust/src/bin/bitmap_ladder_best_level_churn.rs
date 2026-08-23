use low_latency_lab_benchmarks::bitmap_ladder_order_book::OrderBook;
use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Side};
use std::{fs::File, io::Write, time::Instant};
const N: usize = 32_768;
const WARMUP: usize = 10;
const SAMPLES: usize = 200;
fn trace() -> Vec<Event> {
    let mut out = Vec::with_capacity(N * 2);
    for i in 0..N {
        let side = if i % 2 == 0 { Side::Bid } else { Side::Ask };
        let price_ticks = if side == Side::Bid { 100_128 } else { 99_873 };
        let quantity = 1 + i as u64 % 97;
        out.push(Event {
            order_id: i as u64 + 1,
            replacement_order_id: 0,
            price_ticks,
            quantity,
            event_type: EventType::Add,
            side,
        });
        out.push(Event {
            order_id: i as u64 + 1,
            replacement_order_id: 0,
            price_ticks,
            quantity,
            event_type: EventType::Execute,
            side,
        });
    }
    out
}
fn bbo(book: &OrderBook) -> u64 {
    let mut h = 0x9E37_79B9_7F4A_7C15;
    if let Some(x) = book.best_bid() {
        h ^= x.price_ticks ^ (x.quantity << 1)
    }
    if let Some(x) = book.best_ask() {
        h ^= (x.price_ticks << 7) ^ (x.quantity << 3)
    }
    h
}
fn sample(events: &[Event], digest: &mut u64) -> u128 {
    let mut book = OrderBook::new(N, 99_873, 256);
    let start = Instant::now();
    for &e in events {
        book.apply(e).expect("valid trace");
        *digest ^= bbo(&book)
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
    let raw = match args.as_slice() {
        [_] => None,
        [_, f, p] if f == "--raw" => Some(p),
        _ => panic!("usage: bitmap_ladder_best_level_churn [--raw PATH]"),
    };
    let events = trace();
    let layout = OrderBook::new(N, 99_873, 256);
    let mut digest = 0;
    for _ in 0..WARMUP {
        let _ = sample(&events, &mut digest);
    }
    let mut xs = Vec::with_capacity(SAMPLES);
    for _ in 0..SAMPLES {
        xs.push(sample(&events, &mut digest));
    }
    if let Some(path) = raw {
        let mut f = File::create(path).expect("raw");
        writeln!(f, "sample_index,duration_ns").unwrap();
        for (i, x) in xs.iter().enumerate() {
            writeln!(f, "{i},{x}").unwrap();
        }
    }
    let mut s = xs.clone();
    s.sort_unstable();
    let p50 = s[rank(s.len(), 0.5)];
    let mean = xs.iter().sum::<u128>() as f64 / xs.len() as f64;
    println!(
        "format: llab_benchmark_v1\nbenchmark: bitmap_ladder_best_level_churn_v4\nlanguage: rust"
    );
    println!(
        "event_size_bytes: {}\norder_book_size_bytes: {}\norder_index_capacity: {}",
        std::mem::size_of::<Event>(),
        std::mem::size_of::<OrderBook>(),
        layout.order_index_capacity()
    );
    println!(
        "orders_per_sample: {N}\nevents_per_sample: {}\nwarmup_samples: {WARMUP}\nmeasured_samples: {SAMPLES}",
        events.len()
    );
    println!(
        "min_ns: {}\np50_ns: {p50}\np90_ns: {}\np95_ns: {}\np99_ns: {}\np999_ns: {}\nmax_ns: {}\nmean_ns: {mean:.2}\np50_ns_per_event: {:.2}\np50_events_per_s: {:.2}\nresult_digest: {digest}",
        s[0],
        s[rank(s.len(), 0.9)],
        s[rank(s.len(), 0.95)],
        s[rank(s.len(), 0.99)],
        s[rank(s.len(), 0.999)],
        s[s.len() - 1],
        p50 as f64 / events.len() as f64,
        events.len() as f64 * 1e9 / p50 as f64
    );
}
