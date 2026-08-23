use low_latency_lab_benchmarks::feed_sequence_recovery::{FeedSequenceRecovery, IncrementalResult};
use std::{env, fs::File, io::Write, time::Instant};

const EVENTS: usize = 262_144;
const WARMUPS: usize = 10;
const SAMPLES: usize = 200;

fn run_sample() -> u64 {
    let mut recovery = FeedSequenceRecovery::<u64>::new(0);
    let mut ready = Vec::with_capacity(EVENTS);
    recovery.begin_snapshot(0);
    assert!(recovery.finish_snapshot(&mut ready));

    let start = Instant::now();
    for sequence in 1..=EVENTS as u64 {
        assert_eq!(
            recovery.on_incremental(sequence, sequence, &mut ready),
            IncrementalResult::Apply
        );
    }
    let elapsed = start.elapsed().as_nanos() as u64;
    assert_eq!(ready.len(), EVENTS);
    elapsed
}

fn main() {
    let args: Vec<_> = env::args().collect();
    let raw = match args.as_slice() {
        [_] => None,
        [_, flag, path] if flag == "--raw" => Some(path),
        _ => panic!("usage: feed_sequence_recovery [--raw path]"),
    };
    for _ in 0..WARMUPS {
        std::hint::black_box(run_sample());
    }
    let mut values: Vec<_> = (0..SAMPLES).map(|_| run_sample()).collect();
    if let Some(path) = raw {
        let mut output = File::create(path).unwrap();
        for value in &values {
            writeln!(output, "{value}").unwrap();
        }
    }
    values.sort_unstable();
    println!(
        "samples={} p50_ns={} p99_ns={}",
        values.len(),
        values[values.len() / 2],
        values[values.len() * 99 / 100]
    );
}
