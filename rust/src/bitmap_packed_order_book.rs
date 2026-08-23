use crate::fixed_order_index::FixedOrderIndex;
use crate::parity_order_book::{Event, EventType, Level, OrderId, Price, Quantity, Side};

const PRICE_COUNT: usize = 256;
const BITMAP_WORDS: usize = PRICE_COUNT / 64;

#[derive(Clone, Copy)]
struct Order {
    side_and_offset: u64,
    quantity: Quantity,
}
const _: () = assert!(std::mem::size_of::<Order>() == 16);
impl Default for Order {
    fn default() -> Self {
        Self { side_and_offset: 0, quantity: 0 }
    }
}
impl Order {
    const ASK_BIT: u64 = 1 << 63;
    fn make(side: Side, offset: usize, quantity: Quantity) -> Self {
        Self {
            side_and_offset: offset as u64 | if side == Side::Ask { Self::ASK_BIT } else { 0 },
            quantity,
        }
    }
    fn side(self) -> Side {
        if self.side_and_offset & Self::ASK_BIT != 0 { Side::Ask } else { Side::Bid }
    }
    fn offset(self) -> usize {
        (self.side_and_offset & !Self::ASK_BIT) as usize
    }
}
pub struct OrderBook {
    orders: FixedOrderIndex<Order>,
    bids: [Quantity; PRICE_COUNT],
    asks: [Quantity; PRICE_COUNT],
    bid_occupied: [u64; BITMAP_WORDS],
    ask_occupied: [u64; BITMAP_WORDS],
    minimum: Price,
    live: usize,
}
impl OrderBook {
    pub fn new(max: usize, minimum: Price, count: usize) -> Self {
        assert!(
            count == PRICE_COUNT && minimum <= u64::MAX - (PRICE_COUNT as u64 - 1),
            "v6 requires 256 valid prices"
        );
        Self {
            orders: FixedOrderIndex::new(max),
            bids: [0; PRICE_COUNT],
            asks: [0; PRICE_COUNT],
            bid_occupied: [0; BITMAP_WORDS],
            ask_occupied: [0; BITMAP_WORDS],
            minimum,
            live: 0,
        }
    }
    pub fn apply(&mut self, e: Event) -> Result<(), &'static str> {
        match e.event_type {
            EventType::Add => self.add(e.order_id, e.side, e.price_ticks, e.quantity),
            EventType::Cancel => self.reduce(e.order_id, e.quantity, false),
            EventType::Execute => self.reduce(e.order_id, e.quantity, true),
            EventType::Delete | EventType::OrderDelete => self.remove(e.order_id),
            EventType::Replace => {
                if e.order_id == e.replacement_order_id {
                    return Err("replace id");
                }
                self.remove(e.order_id)?;
                self.add(e.replacement_order_id, e.side, e.price_ticks, e.quantity)
            }
            EventType::OrderUpsert => self.upsert(e.order_id, e.side, e.price_ticks, e.quantity),
        }
    }
    pub fn best_bid(&self) -> Option<Level> {
        self.best(Side::Bid)
    }
    pub fn best_ask(&self) -> Option<Level> {
        self.best(Side::Ask)
    }
    pub fn live_order_count(&self) -> usize {
        self.live
    }
    pub fn order_index_capacity(&self) -> usize {
        self.orders.capacity()
    }
    pub fn order_index_total_probes(&self) -> usize {
        self.orders.probe_summary().total_probes
    }
    pub fn order_index_maximum_probes(&self) -> usize {
        self.orders.probe_summary().maximum_probes
    }
    pub fn state_digest(&self) -> u64 {
        let mut h = 14_695_981_039_346_656_037u64;
        hash_u(&mut h, self.live as u64);
        for l in [&self.bids, &self.asks] {
            hash_u(&mut h, l.iter().filter(|&&q| q != 0).count() as u64);
            for (i, &q) in l.iter().enumerate() {
                if q != 0 {
                    hash_u(&mut h, self.minimum + i as u64);
                    hash_u(&mut h, q)
                }
            }
        }
        let mut entries = Vec::with_capacity(self.orders.size());
        self.orders.for_each(|id, order| entries.push((id, order)));
        entries.sort_unstable_by_key(|entry| entry.0);
        for (id, order) in entries {
            hash_u(&mut h, id);
            h = (h ^ (order.side() as u8 as u64)).wrapping_mul(1_099_511_628_211);
            hash_u(&mut h, self.minimum + order.offset() as u64);
            hash_u(&mut h, order.quantity)
        }
        h
    }
    fn offset(&self, p: Price) -> Result<usize, &'static str> {
        if p < self.minimum {
            return Err("price");
        }
        let i = (p - self.minimum) as usize;
        if i >= PRICE_COUNT { Err("price") } else { Ok(i) }
    }
    fn ladder(&self, side: Side) -> &[Quantity; PRICE_COUNT] {
        if side == Side::Bid { &self.bids } else { &self.asks }
    }
    fn ladder_mut(&mut self, side: Side) -> &mut [Quantity; PRICE_COUNT] {
        if side == Side::Bid { &mut self.bids } else { &mut self.asks }
    }
    fn occupancy(&self, side: Side) -> &[u64; BITMAP_WORDS] {
        if side == Side::Bid { &self.bid_occupied } else { &self.ask_occupied }
    }
    fn occupancy_mut(&mut self, side: Side) -> &mut [u64; BITMAP_WORDS] {
        if side == Side::Bid { &mut self.bid_occupied } else { &mut self.ask_occupied }
    }
    fn best(&self, side: Side) -> Option<Level> {
        let map = self.occupancy(side);
        let index = if side == Side::Bid {
            (0..BITMAP_WORDS).rev().find_map(|word| {
                let bits = map[word];
                (bits != 0).then(|| word * 64 + 63 - bits.leading_zeros() as usize)
            })
        } else {
            (0..BITMAP_WORDS).find_map(|word| {
                let bits = map[word];
                (bits != 0).then(|| word * 64 + bits.trailing_zeros() as usize)
            })
        };
        index
            .map(|i| Level { price_ticks: self.minimum + i as u64, quantity: self.ladder(side)[i] })
    }
    fn add(&mut self, id: OrderId, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        if id == 0 || q == 0 {
            return Err("add");
        }
        let offset = self.offset(p)?;
        if !self.orders.insert(id, Order::make(side, offset, q)) {
            return Err("index");
        }
        self.add_level(side, p, q)?;
        self.live += 1;
        Ok(())
    }
    fn remove(&mut self, id: OrderId) -> Result<(), &'static str> {
        let o = *self.orders.find(id).ok_or("id")?;
        self.subtract_level(o.side(), self.minimum + o.offset() as u64, o.quantity)?;
        self.orders.erase(id);
        self.live -= 1;
        Ok(())
    }
    fn reduce(&mut self, id: OrderId, q: Quantity, full: bool) -> Result<(), &'static str> {
        let o = *self.orders.find(id).ok_or("id")?;
        if q == 0 || q > o.quantity || (!full && q == o.quantity) {
            return Err("reduce");
        }
        self.subtract_level(o.side(), self.minimum + o.offset() as u64, q)?;
        if q == o.quantity {
            self.orders.erase(id);
            self.live -= 1
        } else {
            self.orders.find_mut(id).unwrap().quantity -= q
        }
        Ok(())
    }
    fn upsert(
        &mut self,
        id: OrderId,
        side: Side,
        p: Price,
        q: Quantity,
    ) -> Result<(), &'static str> {
        if id == 0 || q == 0 {
            return Err("upsert");
        }
        if let Some(o) = self.orders.find(id).copied() {
            self.subtract_level(o.side(), self.minimum + o.offset() as u64, o.quantity)?;
            self.add_level(side, p, q)?;
            *self.orders.find_mut(id).unwrap() = Order::make(side, self.offset(p)?, q);
            Ok(())
        } else {
            self.add(id, side, p, q)
        }
    }
    fn add_level(&mut self, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        let i = self.offset(p)?;
        let was_empty = self.ladder(side)[i] == 0;
        let level = &mut self.ladder_mut(side)[i];
        *level = level.checked_add(q).ok_or("overflow")?;
        if was_empty {
            self.occupancy_mut(side)[i / 64] |= 1u64 << (i % 64)
        }
        Ok(())
    }
    fn subtract_level(&mut self, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        let i = self.offset(p)?;
        if q == 0 || q > self.ladder(side)[i] {
            return Err("level");
        }
        let becomes_empty = {
            let level = &mut self.ladder_mut(side)[i];
            *level -= q;
            *level == 0
        };
        if becomes_empty {
            self.occupancy_mut(side)[i / 64] &= !(1u64 << (i % 64))
        }
        Ok(())
    }
}
fn hash_u(h: &mut u64, x: u64) {
    for s in (0..64).step_by(8) {
        *h = (*h ^ ((x >> s) as u8 as u64)).wrapping_mul(1_099_511_628_211)
    }
}
