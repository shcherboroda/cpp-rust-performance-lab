use crate::raw_frame_record::RawFrameRecord;
use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicUsize, Ordering};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PushError {
    Full,
    PayloadTooLarge,
}

#[derive(Clone, Copy, Debug)]
pub struct RawFrameHeader {
    pub capture_index: u64,
    pub monotonic_ns: u64,
    pub utc_ns: i64,
    pub connection_id: u64,
    pub direction: u8,
    pub kind: u8,
}

struct Slot {
    header: UnsafeCell<RawFrameHeader>,
    payload_len: UnsafeCell<usize>,
    payload: UnsafeCell<Vec<u8>>,
}

impl Slot {
    fn new(payload_capacity: usize) -> Self {
        Self {
            header: UnsafeCell::new(RawFrameHeader {
                capture_index: 0,
                monotonic_ns: 0,
                utc_ns: 0,
                connection_id: 0,
                direction: 0,
                kind: 0,
            }),
            payload_len: UnsafeCell::new(0),
            payload: UnsafeCell::new(vec![0; payload_capacity]),
        }
    }
}

pub struct RawFrameSpscQueue {
    slots: Box<[Slot]>,
    payload_capacity: usize,
    write_index: AtomicUsize,
    read_index: AtomicUsize,
}

unsafe impl Send for RawFrameSpscQueue {}
unsafe impl Sync for RawFrameSpscQueue {}

impl RawFrameSpscQueue {
    pub fn new(slot_count: usize, payload_capacity: usize) -> Self {
        assert!(slot_count >= 2, "queue needs at least two slots");
        assert!(payload_capacity > 0, "queue payload capacity must be non-zero");
        Self {
            slots: (0..slot_count).map(|_| Slot::new(payload_capacity)).collect(),
            payload_capacity,
            write_index: AtomicUsize::new(0),
            read_index: AtomicUsize::new(0),
        }
    }

    pub fn try_push(&self, header: RawFrameHeader, payload: &[u8]) -> Result<(), PushError> {
        if payload.len() > self.payload_capacity {
            return Err(PushError::PayloadTooLarge);
        }
        let write = self.write_index.load(Ordering::Relaxed);
        let next = self.increment(write);
        if next == self.read_index.load(Ordering::Acquire) {
            return Err(PushError::Full);
        }

        let slot = &self.slots[write];
        // SAFETY: one producer owns this slot until release-publishing write_index.
        unsafe {
            *slot.header.get() = header;
            *slot.payload_len.get() = payload.len();
            (&mut *slot.payload.get())[..payload.len()].copy_from_slice(payload);
        }
        self.write_index.store(next, Ordering::Release);
        Ok(())
    }

    pub fn try_pop_into(&self, out: &mut RawFrameRecord) -> bool {
        let read = self.read_index.load(Ordering::Relaxed);
        if read == self.write_index.load(Ordering::Acquire) {
            return false;
        }
        let slot = &self.slots[read];
        // SAFETY: one consumer owns this published slot until release-publishing read_index.
        unsafe {
            let payload_len = *slot.payload_len.get();
            if out.payload.capacity() < payload_len {
                return false;
            }
            let header = *slot.header.get();
            out.capture_index = header.capture_index;
            out.monotonic_ns = header.monotonic_ns;
            out.utc_ns = header.utc_ns;
            out.connection_id = header.connection_id;
            out.direction = header.direction;
            out.kind = header.kind;
            out.payload.clear();
            out.payload.extend_from_slice(&(&*slot.payload.get())[..payload_len]);
        }
        self.read_index.store(self.increment(read), Ordering::Release);
        true
    }

    pub const fn payload_capacity(&self) -> usize {
        self.payload_capacity
    }

    fn increment(&self, index: usize) -> usize {
        if index + 1 == self.slots.len() { 0 } else { index + 1 }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn header(index: u64) -> RawFrameHeader {
        RawFrameHeader {
            capture_index: index,
            monotonic_ns: index + 1,
            utc_ns: index as i64 + 2,
            connection_id: index + 3,
            direction: 0,
            kind: 0,
        }
    }

    #[test]
    fn preserves_order_and_refuses_overflow() {
        let queue = RawFrameSpscQueue::new(3, 8);
        assert_eq!(queue.try_push(header(1), b"abc"), Ok(()));
        assert_eq!(queue.try_push(header(5), b"de"), Ok(()));
        assert_eq!(queue.try_push(header(9), b"f"), Err(PushError::Full));
        assert_eq!(queue.try_push(header(9), b"123456789"), Err(PushError::PayloadTooLarge));

        let mut out = RawFrameRecord {
            capture_index: 0,
            monotonic_ns: 0,
            utc_ns: 0,
            connection_id: 0,
            direction: 0,
            kind: 0,
            payload: Vec::with_capacity(queue.payload_capacity()),
        };
        assert!(queue.try_pop_into(&mut out));
        assert_eq!(out.capture_index, 1);
        assert_eq!(out.payload, b"abc");
        assert!(queue.try_pop_into(&mut out));
        assert_eq!(out.capture_index, 5);
        assert_eq!(out.payload, b"de");
        assert!(!queue.try_pop_into(&mut out));
    }
}
