use std::collections::{BTreeMap, HashMap};
use std::hash::{BuildHasherDefault, Hasher};

pub type OrderId = u64;
pub type Price = u64;
pub type Quantity = u64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Side {
    Bid,
    Ask,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Event {
    Add {
        order_id: OrderId,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    },
    Cancel {
        order_id: OrderId,
        quantity: Quantity,
    },
    Execute {
        order_id: OrderId,
        quantity: Quantity,
    },
    Delete {
        order_id: OrderId,
    },
    Replace {
        old_order_id: OrderId,
        new_order_id: OrderId,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    },
    OrderUpsert {
        order_id: OrderId,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    },
    OrderDelete {
        order_id: OrderId,
    },
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

#[derive(Default)]
struct StableOrderIdHasher {
    hash: u64,
}

impl Hasher for StableOrderIdHasher {
    fn finish(&self) -> u64 {
        self.hash
    }

    fn write(&mut self, bytes: &[u8]) {
        let mut value = 14_695_981_039_346_656_037_u64;
        for &byte in bytes {
            value ^= u64::from(byte);
            value = value.wrapping_mul(1_099_511_628_211);
        }
        self.hash = mix_order_id(value);
    }

    fn write_u64(&mut self, order_id: u64) {
        self.hash = mix_order_id(order_id);
    }
}

type OrderIndex = HashMap<OrderId, Order, BuildHasherDefault<StableOrderIdHasher>>;

#[derive(Default)]
pub struct OrderBook {
    orders: OrderIndex,
    bids: BTreeMap<Price, Quantity>,
    asks: BTreeMap<Price, Quantity>,
}

impl OrderBook {
    pub fn with_order_capacity(expected_order_capacity: usize) -> Self {
        Self {
            orders: HashMap::with_capacity_and_hasher(
                expected_order_capacity,
                BuildHasherDefault::default(),
            ),
            ..Self::default()
        }
    }

    pub fn apply(&mut self, event: Event) -> Result<(), &'static str> {
        match event {
            Event::Add { order_id, side, price_ticks, quantity } => {
                self.add_order(order_id, side, price_ticks, quantity)
            }
            Event::Cancel { order_id, quantity } => {
                self.subtract_from_order(order_id, quantity, false)
            }
            Event::Execute { order_id, quantity } => {
                self.subtract_from_order(order_id, quantity, true)
            }
            Event::Delete { order_id } | Event::OrderDelete { order_id } => {
                self.remove_order(order_id)
            }
            Event::Replace { old_order_id, new_order_id, side, price_ticks, quantity } => {
                if old_order_id == new_order_id {
                    return Err("replace requires a new order identifier");
                }
                self.remove_order(old_order_id)?;
                self.add_order(new_order_id, side, price_ticks, quantity)
            }
            Event::OrderUpsert { order_id, side, price_ticks, quantity } => {
                self.upsert_order(order_id, side, price_ticks, quantity)
            }
        }
    }

    pub fn best_bid(&self) -> Option<Level> {
        self.bids.last_key_value().map(|(&price_ticks, &quantity)| Level { price_ticks, quantity })
    }

    pub fn best_ask(&self) -> Option<Level> {
        self.asks.first_key_value().map(|(&price_ticks, &quantity)| Level { price_ticks, quantity })
    }

    pub fn level_quantity(&self, side: Side, price_ticks: Price) -> Option<Quantity> {
        self.levels(side).get(&price_ticks).copied()
    }

    pub fn live_order_count(&self) -> usize {
        self.orders.len()
    }

    pub fn order_index_capacity(&self) -> usize {
        self.orders.capacity()
    }

    pub fn state_digest(&self) -> u64 {
        let mut hash = 14_695_981_039_346_656_037_u64;
        hash_u64(&mut hash, self.orders.len() as u64);
        for levels in [&self.bids, &self.asks] {
            hash_u64(&mut hash, levels.len() as u64);
            for (&price, &quantity) in levels {
                hash_u64(&mut hash, price);
                hash_u64(&mut hash, quantity);
            }
        }
        let mut order_ids: Vec<_> = self.orders.keys().copied().collect();
        order_ids.sort_unstable();
        for order_id in order_ids {
            let order = self.orders[&order_id];
            hash_u64(&mut hash, order_id);
            hash_byte(&mut hash, side_byte(order.side));
            hash_u64(&mut hash, order.price_ticks);
            hash_u64(&mut hash, order.remaining_quantity);
        }
        hash
    }

    fn add_order(
        &mut self,
        order_id: OrderId,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        if order_id == 0 || quantity == 0 || self.orders.contains_key(&order_id) {
            return Err("invalid add order");
        }
        self.add_to_level(side, price_ticks, quantity)?;
        self.orders.insert(order_id, Order { side, price_ticks, remaining_quantity: quantity });
        Ok(())
    }

    fn remove_order(&mut self, order_id: OrderId) -> Result<(), &'static str> {
        let order = self.orders.remove(&order_id).ok_or("unknown order identifier")?;
        self.subtract_from_level(order.side, order.price_ticks, order.remaining_quantity)
    }

    fn subtract_from_order(
        &mut self,
        order_id: OrderId,
        quantity: Quantity,
        allow_full_removal: bool,
    ) -> Result<(), &'static str> {
        let order = *self.orders.get(&order_id).ok_or("unknown order identifier")?;
        if quantity == 0
            || quantity > order.remaining_quantity
            || (!allow_full_removal && quantity == order.remaining_quantity)
        {
            return Err("invalid order reduction");
        }
        self.subtract_from_level(order.side, order.price_ticks, quantity)?;
        if quantity == order.remaining_quantity {
            self.orders.remove(&order_id);
        } else if let Some(existing) = self.orders.get_mut(&order_id) {
            existing.remaining_quantity -= quantity;
        }
        Ok(())
    }

    fn upsert_order(
        &mut self,
        order_id: OrderId,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        if order_id == 0 || quantity == 0 {
            return Err("invalid order upsert");
        }
        if let Some(existing) = self.orders.get(&order_id).copied() {
            self.subtract_from_level(
                existing.side,
                existing.price_ticks,
                existing.remaining_quantity,
            )?;
        }
        self.add_to_level(side, price_ticks, quantity)?;
        self.orders.insert(order_id, Order { side, price_ticks, remaining_quantity: quantity });
        Ok(())
    }

    fn levels(&self, side: Side) -> &BTreeMap<Price, Quantity> {
        match side {
            Side::Bid => &self.bids,
            Side::Ask => &self.asks,
        }
    }

    fn levels_mut(&mut self, side: Side) -> &mut BTreeMap<Price, Quantity> {
        match side {
            Side::Bid => &mut self.bids,
            Side::Ask => &mut self.asks,
        }
    }

    fn add_to_level(
        &mut self,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        let level = self.levels_mut(side).entry(price_ticks).or_default();
        *level = level.checked_add(quantity).ok_or("price-level quantity overflow")?;
        Ok(())
    }

    fn subtract_from_level(
        &mut self,
        side: Side,
        price_ticks: Price,
        quantity: Quantity,
    ) -> Result<(), &'static str> {
        let levels = self.levels_mut(side);
        let level = levels.get_mut(&price_ticks).ok_or("invalid price-level reduction")?;
        if quantity == 0 || quantity > *level {
            return Err("invalid price-level reduction");
        }
        *level -= quantity;
        if *level == 0 {
            levels.remove(&price_ticks);
        }
        Ok(())
    }
}

fn side_byte(side: Side) -> u8 {
    match side {
        Side::Bid => 0,
        Side::Ask => 1,
    }
}

fn mix_order_id(mut value: u64) -> u64 {
    value = value.wrapping_add(0x9E37_79B9_7F4A_7C15);
    value = (value ^ (value >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    value = (value ^ (value >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    value ^ (value >> 31)
}

fn hash_byte(hash: &mut u64, byte: u8) {
    *hash ^= u64::from(byte);
    *hash = hash.wrapping_mul(1_099_511_628_211);
}

fn hash_u64(hash: &mut u64, value: u64) {
    for shift in (0..64).step_by(8) {
        hash_byte(hash, (value >> shift) as u8);
    }
}
