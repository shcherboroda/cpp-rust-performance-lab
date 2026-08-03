use std::time::Instant;

const INPUT_LENGTH: usize = 1_048_576;
const WARMUP_RUNS: usize = 5;
const MEASURED_RUNS: usize = 100;
const SEED: u64 = 0x1234_5678_9ABC_DEF0;
const EXPECTED_CHECKSUM: u64 = 0x839C_D625_000C_DB7A;
const P50_INDEX: usize = 49;
const P90_INDEX: usize = 89;
const P95_INDEX: usize = 94;

struct SplitMix64 {
    state: u64,
}

impl SplitMix64 {
    fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    fn next(&mut self) -> u64 {
        // SplitMix64 transition: state += 0x9E3779B97F4A7C15 (mod 2^64);
        // z = state; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
        // z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31).
        self.state = self.state.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut z = self.state;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        z ^ (z >> 31)
    }
}

fn sequential_sum(values: &[u64]) -> u64 {
    let mut accumulator = 0_u64;
    for &value in values {
        accumulator = accumulator.wrapping_add(value);
    }
    accumulator
}

fn main() {
    let mut values = Vec::with_capacity(INPUT_LENGTH);
    let mut generator = SplitMix64::new(SEED);
    for _ in 0..INPUT_LENGTH {
        values.push(generator.next());
    }

    for _ in 0..WARMUP_RUNS {
        let checksum = sequential_sum(&values);
        assert_eq!(checksum, EXPECTED_CHECKSUM, "checksum validation failed");
    }

    let mut durations_ns = Vec::with_capacity(MEASURED_RUNS);
    let mut checksum = 0_u64;
    for _ in 0..MEASURED_RUNS {
        let start = Instant::now();
        checksum = sequential_sum(&values);
        let elapsed = start.elapsed();

        assert_eq!(checksum, EXPECTED_CHECKSUM, "checksum validation failed");
        durations_ns.push(elapsed.as_nanos());
    }

    let mut sorted_durations = durations_ns.clone();
    sorted_durations.sort_unstable();
    let total_ns: u128 = durations_ns.iter().sum();
    let mean_ns = total_ns as f64 / MEASURED_RUNS as f64;
    let input_bytes = (INPUT_LENGTH * std::mem::size_of::<u64>()) as f64;
    let gib = (1_u64 << 30) as f64;
    let p50_gib_per_s = input_bytes * 1_000_000_000.0 / (gib * sorted_durations[P50_INDEX] as f64);
    let mean_gib_per_s = input_bytes * 1_000_000_000.0 / (gib * mean_ns);

    println!("warmup_runs: {WARMUP_RUNS}");
    println!("sample_count: {MEASURED_RUNS}");
    println!("min_ns: {}", sorted_durations[0]);
    println!("p50_ns: {}", sorted_durations[P50_INDEX]);
    println!("p90_ns: {}", sorted_durations[P90_INDEX]);
    println!("p95_ns: {}", sorted_durations[P95_INDEX]);
    println!("max_ns: {}", sorted_durations[MEASURED_RUNS - 1]);
    println!("mean_ns: {mean_ns:.2}");
    println!("p50_gib_per_s: {p50_gib_per_s:.2}");
    println!("mean_gib_per_s: {mean_gib_per_s:.2}");
    println!("checksum decimal: {checksum}");
    println!("checksum hexadecimal: 0x{checksum:016X}");
}
