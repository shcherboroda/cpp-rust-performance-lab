use crate::parity_order_book::OrderId;

#[derive(Clone, Copy)]
struct Slot<Value: Copy + Default> {
    id: OrderId,
    value: Value,
    occupied: bool,
}

impl<Value: Copy + Default> Default for Slot<Value> {
    fn default() -> Self {
        Self { id: 0, value: Value::default(), occupied: false }
    }
}

/// Fixed-capacity order-ID index with linear probing and backward-shift deletion.
///
/// The index keeps load at or below 50 percent and never allocates after
/// construction. Deletion restores a terminating empty slot instead of leaving
/// a tombstone, so probe chains do not accumulate historical entries.
pub struct FixedOrderIndex<Value: Copy + Default> {
    slots: Vec<Slot<Value>>,
    maximum_live_entries: usize,
    size: usize,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ProbeSummary {
    pub occupied: usize,
    pub total_probes: usize,
    pub maximum_probes: usize,
}

impl<Value: Copy + Default> FixedOrderIndex<Value> {
    pub fn new(maximum_live_entries: usize) -> Self {
        Self {
            slots: vec![Slot::default(); table_capacity(maximum_live_entries)],
            maximum_live_entries,
            size: 0,
        }
    }

    pub fn insert(&mut self, id: OrderId, value: Value) -> bool {
        if id == 0 || self.size == self.maximum_live_entries {
            return false;
        }

        let mask = self.slots.len() - 1;
        let mut index = self.home(id);
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if !slot.occupied {
                self.slots[index] = Slot { id, value, occupied: true };
                self.size += 1;
                return true;
            }
            if slot.id == id {
                return false;
            }
            index = (index + 1) & mask;
        }
        false
    }

    pub fn find(&self, id: OrderId) -> Option<&Value> {
        self.find_slot(id).map(|index| &self.slots[index].value)
    }

    pub fn find_mut(&mut self, id: OrderId) -> Option<&mut Value> {
        let index = self.find_slot(id)?;
        Some(&mut self.slots[index].value)
    }

    pub fn erase(&mut self, id: OrderId) -> Option<Value> {
        let index = self.find_slot(id)?;
        let value = self.slots[index].value;
        self.erase_at(index);
        self.size -= 1;
        Some(value)
    }

    pub fn size(&self) -> usize {
        self.size
    }

    pub fn capacity(&self) -> usize {
        self.slots.len()
    }

    pub fn for_each(&self, mut visitor: impl FnMut(OrderId, Value)) {
        for slot in &self.slots {
            if slot.occupied {
                visitor(slot.id, slot.value);
            }
        }
    }

    pub fn probe_summary(&self) -> ProbeSummary {
        let mask = self.slots.len() - 1;
        let mut summary = ProbeSummary::default();
        for (index, slot) in self.slots.iter().enumerate() {
            if slot.occupied {
                let probes = distance(self.home(slot.id), index, mask) + 1;
                summary.occupied += 1;
                summary.total_probes += probes;
                summary.maximum_probes = summary.maximum_probes.max(probes);
            }
        }
        summary
    }

    fn home(&self, id: OrderId) -> usize {
        mix_order_id(id) as usize & (self.slots.len() - 1)
    }

    fn find_slot(&self, id: OrderId) -> Option<usize> {
        if id == 0 {
            return None;
        }
        let mask = self.slots.len() - 1;
        let mut index = self.home(id);
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if !slot.occupied {
                return None;
            }
            if slot.id == id {
                return Some(index);
            }
            index = (index + 1) & mask;
        }
        None
    }

    fn erase_at(&mut self, index: usize) {
        let mask = self.slots.len() - 1;
        let mut hole = index;
        let mut next = (hole + 1) & mask;
        while self.slots[next].occupied {
            let entry_home = self.home(self.slots[next].id);
            if distance(entry_home, next, mask) >= distance(entry_home, hole, mask) {
                self.slots[hole] = self.slots[next];
                hole = next;
            }
            next = (next + 1) & mask;
        }
        self.slots[hole].occupied = false;
    }
}

fn table_capacity(maximum_live_entries: usize) -> usize {
    maximum_live_entries
        .checked_mul(2)
        .expect("fixed order index capacity overflow")
        .max(8)
        .checked_next_power_of_two()
        .expect("fixed order index capacity overflow")
}

fn mix_order_id(mut value: u64) -> u64 {
    value = value.wrapping_add(0x9E37_79B9_7F4A_7C15);
    value = (value ^ (value >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    value = (value ^ (value >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    value ^ (value >> 31)
}

fn distance(from: usize, to: usize, mask: usize) -> usize {
    to.wrapping_sub(from) & mask
}
