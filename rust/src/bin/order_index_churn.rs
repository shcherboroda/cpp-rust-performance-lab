use low_latency_lab_benchmarks::fixed_order_index::FixedOrderIndex;
use std::{env, fs::File, io::Write, time::Instant};

const LIVE_ORDERS: usize = 16_384;
const CHURN_OPERATIONS: usize = 32_768;
const WARMUPS: usize = 10;
const SAMPLES: usize = 200;

#[derive(Clone, Copy, PartialEq, Eq)]
enum State {
    Empty,
    Occupied,
    Tombstone,
}

#[derive(Clone, Copy)]
struct Slot {
    id: u64,
    _value: u32,
    state: State,
}

struct TombstoneIndex {
    slots: Vec<Slot>,
}

impl TombstoneIndex {
    fn new(maximum_live: usize) -> Self {
        Self { slots: vec![Slot { id: 0, _value: 0, state: State::Empty }; capacity(maximum_live)] }
    }

    fn insert(&mut self, id: u64, value: u32) -> bool {
        let mask = self.slots.len() - 1;
        let mut first_tombstone = None;
        let mut index = mix(id) as usize & mask;
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if slot.state == State::Empty {
                self.slots[first_tombstone.unwrap_or(index)] =
                    Slot { id, _value: value, state: State::Occupied };
                return true;
            }
            if slot.state == State::Tombstone && first_tombstone.is_none() {
                first_tombstone = Some(index);
            }
            if slot.state == State::Occupied && slot.id == id {
                return false;
            }
            index = (index + 1) & mask;
        }
        if let Some(index) = first_tombstone {
            self.slots[index] = Slot { id, _value: value, state: State::Occupied };
            true
        } else {
            false
        }
    }

    fn erase(&mut self, id: u64) -> bool {
        let mask = self.slots.len() - 1;
        let mut index = mix(id) as usize & mask;
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if slot.state == State::Empty {
                return false;
            }
            if slot.state == State::Occupied && slot.id == id {
                self.slots[index].state = State::Tombstone;
                return true;
            }
            index = (index + 1) & mask;
        }
        false
    }
}

trait Index {
    fn insert(&mut self, id: u64, value: u32) -> bool;
    fn erase(&mut self, id: u64) -> bool;
}

impl Index for TombstoneIndex {
    fn insert(&mut self, id: u64, value: u32) -> bool {
        Self::insert(self, id, value)
    }

    fn erase(&mut self, id: u64) -> bool {
        Self::erase(self, id)
    }
}

impl Index for FixedOrderIndex<u32> {
    fn insert(&mut self, id: u64, value: u32) -> bool {
        Self::insert(self, id, value)
    }

    fn erase(&mut self, id: u64) -> bool {
        Self::erase(self, id).is_some()
    }
}

fn run_sample<I: Index>(mut index: I) -> u64 {
    let mut live = vec![0_u64; LIVE_ORDERS];
    for (position, id) in live.iter_mut().enumerate() {
        *id = position as u64 + 1;
        assert!(index.insert(*id, position as u32));
    }

    let start = Instant::now();
    let mut digest = 0_u64;
    for operation in 0..CHURN_OPERATIONS {
        let victim = (operation * 40_503) % LIVE_ORDERS;
        assert!(index.erase(live[victim]));
        let replacement = 1_000_000 + operation as u64;
        assert!(index.insert(replacement, operation as u32));
        live[victim] = replacement;
        digest ^= replacement;
    }
    std::hint::black_box(digest);
    start.elapsed().as_nanos() as u64
}

fn collect<I: Index>(make: impl Fn() -> I) -> Vec<u64> {
    for _ in 0..WARMUPS {
        std::hint::black_box(run_sample(make()));
    }
    (0..SAMPLES).map(|_| run_sample(make())).collect()
}

fn capacity(maximum_live: usize) -> usize {
    maximum_live.checked_mul(2).unwrap().max(8).next_power_of_two()
}

fn mix(mut value: u64) -> u64 {
    match option_env!("CARGO_BIN_NAME").unwrap_or("") {
        "identity_tombstone_order_index_churn" => return value,
        "multiplicative_tombstone_order_index_churn" => {
            return value.wrapping_mul(0x9E37_79B9_7F4A_7C15);
        }
        _ => {}
    }
    value = value.wrapping_add(0x9E37_79B9_7F4A_7C15);
    value = (value ^ (value >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    value = (value ^ (value >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    value ^ (value >> 31)
}

pub fn main() {
    let args: Vec<_> = env::args().collect();
    let raw = match args.as_slice() {
        [_] => None,
        [_, flag, path] if flag == "--raw" => Some(path),
        _ => panic!("usage: order_index_churn [--raw path]"),
    };
    let backshift =
        env::current_exe().unwrap().file_name().unwrap().to_string_lossy().starts_with("backshift");
    let mut values = if backshift {
        collect(|| FixedOrderIndex::new(LIVE_ORDERS))
    } else {
        collect(|| TombstoneIndex::new(LIVE_ORDERS))
    };
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
