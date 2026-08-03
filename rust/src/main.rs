const INPUT_LENGTH: usize = 1_048_576;
const SEED: u64 = 0x1234_5678_9ABC_DEF0;
const EXPECTED_CHECKSUM: u64 = 0x839C_D625_000C_DB7A;

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

    let checksum = sequential_sum(&values);
    assert_eq!(checksum, EXPECTED_CHECKSUM, "checksum validation failed");

    println!("checksum decimal: {checksum}");
    println!("checksum hexadecimal: 0x{checksum:016X}");
}
