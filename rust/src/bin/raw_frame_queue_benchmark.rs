use low_latency_lab_benchmarks::raw_frame_queue::{RawFrameHeader, RawFrameSpscQueue};
use low_latency_lab_benchmarks::raw_frame_record::RawFrameRecord;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::thread;
use std::time::Instant;

const EVENTS: u64 = 1_000_000;
const SAMPLES: usize = 20;
const PAYLOAD_CAPACITY: usize = 4096;
const PRODUCER_CPU: usize = 0;
const CONSUMER_CPU: usize = 2;

fn pin_current_thread(cpu: usize) {
    // SAFETY: the set is initialized before passing it to the pthread API.
    unsafe {
        let mut set: libc::cpu_set_t = std::mem::zeroed();
        libc::CPU_ZERO(&mut set);
        libc::CPU_SET(cpu, &mut set);
        assert_eq!(
            libc::pthread_setaffinity_np(
                libc::pthread_self(),
                std::mem::size_of::<libc::cpu_set_t>(),
                &set
            ),
            0
        );
    }
}

fn rank(values: &mut [f64], percentile: f64) -> f64 {
    values.sort_by(f64::total_cmp);
    values[((values.len() as f64 * percentile).ceil() as usize - 1).min(values.len() - 1)]
}

fn main() {
    let mut samples = Vec::with_capacity(SAMPLES);
    for _ in 0..SAMPLES {
        let queue = Arc::new(RawFrameSpscQueue::new(1024, PAYLOAD_CAPACITY));
        let start = Arc::new(AtomicBool::new(false));
        let ready = Arc::new(AtomicU64::new(0));
        let consumed = Arc::new(AtomicU64::new(0));
        let producer_queue = Arc::clone(&queue);
        let producer_start = Arc::clone(&start);
        let producer_ready = Arc::clone(&ready);
        let producer = thread::spawn(move || {
            pin_current_thread(PRODUCER_CPU);
            producer_ready.fetch_add(1, Ordering::Release);
            let small = [0xA5_u8; 64];
            let large = [0x5A_u8; 2048];
            while !producer_start.load(Ordering::Acquire) {
                std::hint::spin_loop();
            }
            for capture_index in 0..EVENTS {
                let payload = if capture_index % 16 == 0 { &large[..] } else { &small[..] };
                let header = RawFrameHeader {
                    capture_index,
                    monotonic_ns: capture_index,
                    utc_ns: 0,
                    connection_id: 1,
                    direction: 0,
                    kind: 0,
                };
                while producer_queue.try_push(header, payload).is_err() {
                    std::hint::spin_loop();
                }
            }
        });
        let consumer_queue = Arc::clone(&queue);
        let consumer_start = Arc::clone(&start);
        let consumer_ready = Arc::clone(&ready);
        let consumer_count = Arc::clone(&consumed);
        let consumer = thread::spawn(move || {
            pin_current_thread(CONSUMER_CPU);
            consumer_ready.fetch_add(1, Ordering::Release);
            let mut out = RawFrameRecord {
                capture_index: 0,
                monotonic_ns: 0,
                utc_ns: 0,
                connection_id: 0,
                direction: 0,
                kind: 0,
                payload: Vec::with_capacity(PAYLOAD_CAPACITY),
            };
            while !consumer_start.load(Ordering::Acquire) {
                std::hint::spin_loop();
            }
            for expected in 0..EVENTS {
                while !consumer_queue.try_pop_into(&mut out) {
                    std::hint::spin_loop();
                }
                assert_eq!(out.capture_index, expected);
            }
            consumer_count.store(EVENTS, Ordering::Release);
        });
        while ready.load(Ordering::Acquire) != 2 {
            std::hint::spin_loop();
        }
        let started = Instant::now();
        start.store(true, Ordering::Release);
        producer.join().unwrap();
        consumer.join().unwrap();
        assert_eq!(consumed.load(Ordering::Acquire), EVENTS);
        samples.push(started.elapsed().as_nanos() as f64 / EVENTS as f64);
    }
    let mut p50 = samples.clone();
    let mut p99 = samples.clone();
    println!(
        "samples={} events_per_sample={} producer_cpu={} consumer_cpu={} payload=15x64B+1x2048B p50_ns_per_message={:.3} p99_ns_per_message={:.3}",
        SAMPLES,
        EVENTS,
        PRODUCER_CPU,
        CONSUMER_CPU,
        rank(&mut p50, 0.50),
        rank(&mut p99, 0.99)
    );
}
