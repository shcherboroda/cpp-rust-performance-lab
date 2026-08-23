pub type OrderId = u64;
pub type Price = u64;
pub type Quantity = u64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum Side {
    Bid = 0,
    Ask = 1,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum EventType {
    Add,
    Cancel,
    Execute,
    Delete,
    Replace,
    OrderUpsert,
    OrderDelete,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(C)]
pub struct Event {
    pub order_id: OrderId,
    pub replacement_order_id: OrderId,
    pub price_ticks: Price,
    pub quantity: Quantity,
    pub event_type: EventType,
    pub side: Side,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Level {
    pub price_ticks: Price,
    pub quantity: Quantity,
}

#[derive(Clone, Copy)]
struct Order {
    side: Side,
    price_ticks: Price,
    remaining_quantity: Quantity,
}

#[derive(Clone, Copy, Eq, PartialEq)]
enum SlotState {
    Empty,
    Occupied,
    Tombstone,
}

#[derive(Clone, Copy)]
struct Slot {
    order_id: OrderId,
    order: Order,
    state: SlotState,
}

impl Default for Slot {
    fn default() -> Self {
        Self {
            order_id: 0,
            order: Order { side: Side::Bid, price_ticks: 0, remaining_quantity: 0 },
            state: SlotState::Empty,
        }
    }
}

pub struct OrderBook {
    slots: Vec<Slot>,
    bids: Vec<Level>,
    asks: Vec<Level>,
    live_order_count: usize,
}

impl OrderBook {
    pub fn new(maximum_live_orders: usize) -> Self {
        Self {
            slots: vec![Slot::default(); table_capacity(maximum_live_orders)],
            bids: Vec::new(),
            asks: Vec::new(),
            live_order_count: 0,
        }
    }

    pub fn apply(&mut self, event: Event) -> Result<(), &'static str> {
        match event.event_type {
            EventType::Add => {
                self.add_order(event.order_id, event.side, event.price_ticks, event.quantity)
            }
            EventType::Cancel => self.subtract_from_order(event.order_id, event.quantity, false),
            EventType::Execute => self.subtract_from_order(event.order_id, event.quantity, true),
            EventType::Delete | EventType::OrderDelete => self.remove_order(event.order_id),
            EventType::Replace => {
                if event.order_id == event.replacement_order_id {
                    return Err("replace requires new id");
                }
                self.remove_order(event.order_id)?;
                self.add_order(
                    event.replacement_order_id,
                    event.side,
                    event.price_ticks,
                    event.quantity,
                )
            }
            EventType::OrderUpsert => {
                self.upsert_order(event.order_id, event.side, event.price_ticks, event.quantity)
            }
        }
    }

    pub fn best_bid(&self) -> Option<Level> {
        self.bids.last().copied()
    }
    pub fn best_ask(&self) -> Option<Level> {
        self.asks.first().copied()
    }
    pub fn level_quantity(&self, side: Side, price: Price) -> Option<Quantity> {
        let levels = self.levels(side);
        levels
            .binary_search_by_key(&price, |level| level.price_ticks)
            .ok()
            .map(|index| levels[index].quantity)
    }
    pub fn live_order_count(&self) -> usize {
        self.live_order_count
    }
    pub fn order_index_capacity(&self) -> usize {
        self.slots.len()
    }

    pub fn state_digest(&self) -> u64 {
        let mut hash = 14_695_981_039_346_656_037_u64;
        hash_u64(&mut hash, self.live_order_count as u64);
        for levels in [&self.bids, &self.asks] {
            hash_u64(&mut hash, levels.len() as u64);
            for level in levels {
                hash_u64(&mut hash, level.price_ticks);
                hash_u64(&mut hash, level.quantity);
            }
        }
        let mut ids: Vec<_> = self
            .slots
            .iter()
            .filter(|slot| slot.state == SlotState::Occupied)
            .map(|slot| slot.order_id)
            .collect();
        ids.sort_unstable();
        for id in ids {
            let order = self.require_slot(id).unwrap().order;
            hash_u64(&mut hash, id);
            hash_byte(&mut hash, order.side as u8);
            hash_u64(&mut hash, order.price_ticks);
            hash_u64(&mut hash, order.remaining_quantity);
        }
        hash
    }

    fn find_slot(&self, id: OrderId) -> Option<usize> {
        let mask = self.slots.len() - 1;
        let mut index = mix_order_id(id) as usize & mask;
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if slot.state == SlotState::Empty {
                return None;
            }
            if slot.state == SlotState::Occupied && slot.order_id == id {
                return Some(index);
            }
            index = (index + 1) & mask;
        }
        None
    }

    fn insertion_slot(&self, id: OrderId) -> Result<usize, &'static str> {
        let mask = self.slots.len() - 1;
        let mut index = mix_order_id(id) as usize & mask;
        let mut first_tombstone = None;
        for _ in 0..self.slots.len() {
            let slot = self.slots[index];
            if slot.state == SlotState::Empty {
                return Ok(first_tombstone.unwrap_or(index));
            }
            if slot.state == SlotState::Tombstone && first_tombstone.is_none() {
                first_tombstone = Some(index);
            }
            if slot.state == SlotState::Occupied && slot.order_id == id {
                return Err("duplicate id or full index");
            }
            index = (index + 1) & mask;
        }
        first_tombstone.ok_or("duplicate id or full index")
    }

    fn require_slot(&self, id: OrderId) -> Result<&Slot, &'static str> {
        self.find_slot(id).map(|index| &self.slots[index]).ok_or("unknown order id")
    }
    fn require_slot_mut(&mut self, id: OrderId) -> Result<&mut Slot, &'static str> {
        let index = self.find_slot(id).ok_or("unknown order id")?;
        Ok(&mut self.slots[index])
    }
    fn levels(&self, side: Side) -> &[Level] {
        match side {
            Side::Bid => &self.bids,
            Side::Ask => &self.asks,
        }
    }
    fn levels_mut(&mut self, side: Side) -> &mut Vec<Level> {
        match side {
            Side::Bid => &mut self.bids,
            Side::Ask => &mut self.asks,
        }
    }

    fn add_order(
        &mut self,
        id: OrderId,
        side: Side,
        price: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        if id == 0 || quantity == 0 {
            return Err("invalid add");
        }
        let index = self.insertion_slot(id)?;
        self.add_to_level(side, price, quantity)?;
        self.slots[index] = Slot {
            order_id: id,
            order: Order { side, price_ticks: price, remaining_quantity: quantity },
            state: SlotState::Occupied,
        };
        self.live_order_count += 1;
        Ok(())
    }
    fn remove_order(&mut self, id: OrderId) -> Result<(), &'static str> {
        let order = self.require_slot(id)?.order;
        self.subtract_from_level(order.side, order.price_ticks, order.remaining_quantity)?;
        self.require_slot_mut(id)?.state = SlotState::Tombstone;
        self.live_order_count -= 1;
        Ok(())
    }
    fn subtract_from_order(
        &mut self,
        id: OrderId,
        quantity: Quantity,
        allow_full_removal: bool,
    ) -> Result<(), &'static str> {
        let order = self.require_slot(id)?.order;
        if quantity == 0
            || quantity > order.remaining_quantity
            || (!allow_full_removal && quantity == order.remaining_quantity)
        {
            return Err("invalid reduction");
        }
        self.subtract_from_level(order.side, order.price_ticks, quantity)?;
        let slot = self.require_slot_mut(id)?;
        if quantity == order.remaining_quantity {
            slot.state = SlotState::Tombstone;
            self.live_order_count -= 1;
        } else {
            slot.order.remaining_quantity -= quantity;
        }
        Ok(())
    }
    fn upsert_order(
        &mut self,
        id: OrderId,
        side: Side,
        price: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        if id == 0 || quantity == 0 {
            return Err("invalid upsert");
        }
        if let Some(index) = self.find_slot(id) {
            let old = self.slots[index].order;
            self.subtract_from_level(old.side, old.price_ticks, old.remaining_quantity)?;
            self.add_to_level(side, price, quantity)?;
            self.slots[index].order =
                Order { side, price_ticks: price, remaining_quantity: quantity };
            Ok(())
        } else {
            self.add_order(id, side, price, quantity)
        }
    }
    fn add_to_level(
        &mut self,
        side: Side,
        price: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        let levels = self.levels_mut(side);
        match levels.binary_search_by_key(&price, |level| level.price_ticks) {
            Ok(index) => {
                levels[index].quantity =
                    levels[index].quantity.checked_add(quantity).ok_or("level overflow")?;
            }
            Err(index) => levels.insert(index, Level { price_ticks: price, quantity }),
        }
        Ok(())
    }
    fn subtract_from_level(
        &mut self,
        side: Side,
        price: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        let levels = self.levels_mut(side);
        let index = levels
            .binary_search_by_key(&price, |level| level.price_ticks)
            .map_err(|_| "invalid level reduction")?;
        if quantity == 0 || quantity > levels[index].quantity {
            return Err("invalid level reduction");
        }
        levels[index].quantity -= quantity;
        if levels[index].quantity == 0 {
            levels.remove(index);
        }
        Ok(())
    }
}

fn table_capacity(maximum_live_orders: usize) -> usize {
    maximum_live_orders
        .checked_mul(2)
        .unwrap_or(usize::MAX)
        .max(8)
        .checked_next_power_of_two()
        .expect("capacity overflow")
}
fn mix_order_id(mut value: u64) -> u64 {
    value = value.wrapping_add(0x9E37_79B9_7F4A_7C15);
    value = (value ^ (value >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    value = (value ^ (value >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    value ^ (value >> 31)
}
fn hash_byte(hash: &mut u64, byte: u8) {
    *hash = (*hash ^ u64::from(byte)).wrapping_mul(1_099_511_628_211);
}
fn hash_u64(hash: &mut u64, value: u64) {
    for shift in (0..64).step_by(8) {
        hash_byte(hash, (value >> shift) as u8);
    }
}
