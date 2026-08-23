use low_latency_lab_benchmarks::bitmap_ladder_order_book;
use low_latency_lab_benchmarks::bitmap_packed_order_book;
use low_latency_lab_benchmarks::parity_order_book::{Event, EventType, Side};
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

struct CountingAllocator;

static COUNT_ALLOCATIONS: AtomicBool = AtomicBool::new(false);
static ALLOCATION_CALLS: AtomicUsize = AtomicUsize::new(0);

unsafe impl GlobalAlloc for CountingAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if COUNT_ALLOCATIONS.load(Ordering::Relaxed) {
            ALLOCATION_CALLS.fetch_add(1, Ordering::Relaxed);
        }
        unsafe { System.alloc(layout) }
    }

    unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
        unsafe { System.dealloc(pointer, layout) }
    }
}

#[global_allocator]
static ALLOCATOR: CountingAllocator = CountingAllocator;

pub fn main() {
    let packed = option_env!("CARGO_BIN_NAME") == Some("bitmap_packed_allocation_audit");
    if packed {
        audit(bitmap_packed_order_book::OrderBook::new(32_768, 99_873, 256));
    } else {
        audit(bitmap_ladder_order_book::OrderBook::new(32_768, 99_873, 256));
    }
}

trait AuditBook {
    fn apply(&mut self, event: Event) -> Result<(), &'static str>;
    fn live_order_count(&self) -> usize;
}

impl AuditBook for bitmap_ladder_order_book::OrderBook {
    fn apply(&mut self, event: Event) -> Result<(), &'static str> {
        self.apply(event)
    }
    fn live_order_count(&self) -> usize {
        self.live_order_count()
    }
}

impl AuditBook for bitmap_packed_order_book::OrderBook {
    fn apply(&mut self, event: Event) -> Result<(), &'static str> {
        self.apply(event)
    }
    fn live_order_count(&self) -> usize {
        self.live_order_count()
    }
}

fn audit<B: AuditBook>(mut book: B) {
    ALLOCATION_CALLS.store(0, Ordering::Relaxed);
    COUNT_ALLOCATIONS.store(true, Ordering::Relaxed);
    for cycle in 0..8_192_u64 {
        let id = cycle * 3 + 1;
        let price = 100_000 + cycle % 128;
        book.apply(Event {
            order_id: id,
            replacement_order_id: 0,
            price_ticks: price,
            quantity: 10,
            event_type: EventType::Add,
            side: Side::Bid,
        })
        .unwrap();
        book.apply(Event {
            order_id: id,
            replacement_order_id: 0,
            price_ticks: 0,
            quantity: 3,
            event_type: EventType::Cancel,
            side: Side::Bid,
        })
        .unwrap();
        book.apply(Event {
            order_id: id,
            replacement_order_id: 0,
            price_ticks: 0,
            quantity: 7,
            event_type: EventType::Execute,
            side: Side::Bid,
        })
        .unwrap();
    }
    COUNT_ALLOCATIONS.store(false, Ordering::Relaxed);
    assert_eq!(book.live_order_count(), 0);
    assert_eq!(ALLOCATION_CALLS.load(Ordering::Relaxed), 0);
}
