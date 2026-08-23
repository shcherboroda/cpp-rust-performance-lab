use crate::parity_order_book::{Event, EventType, Level, OrderId, Price, Quantity, Side};

#[derive(Clone, Copy, PartialEq)]
enum State {
    Empty,
    Occupied,
    Tombstone,
}
#[derive(Clone, Copy)]
struct Order {
    side: Side,
    price: Price,
    quantity: Quantity,
}
#[derive(Clone, Copy)]
struct Slot {
    id: OrderId,
    order: Order,
    state: State,
}
impl Default for Slot {
    fn default() -> Self {
        Self { id: 0, order: Order { side: Side::Bid, price: 0, quantity: 0 }, state: State::Empty }
    }
}
pub struct OrderBook {
    slots: Vec<Slot>,
    bids: Vec<Quantity>,
    asks: Vec<Quantity>,
    minimum: Price,
    best_bid: Option<Price>,
    best_ask: Option<Price>,
    live: usize,
}
impl OrderBook {
    pub fn new(max: usize, minimum: Price, count: usize) -> Self {
        assert!(count > 0 && minimum <= u64::MAX - (count as u64 - 1));
        Self {
            slots: vec![Slot::default(); capacity(max)],
            bids: vec![0; count],
            asks: vec![0; count],
            minimum,
            best_bid: None,
            best_ask: None,
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
        self.best_bid
            .map(|p| Level { price_ticks: p, quantity: self.bids[self.offset(p).unwrap()] })
    }
    pub fn best_ask(&self) -> Option<Level> {
        self.best_ask
            .map(|p| Level { price_ticks: p, quantity: self.asks[self.offset(p).unwrap()] })
    }
    pub fn live_order_count(&self) -> usize {
        self.live
    }
    pub fn order_index_capacity(&self) -> usize {
        self.slots.len()
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
        let mut ids: Vec<_> =
            self.slots.iter().filter(|s| s.state == State::Occupied).map(|s| s.id).collect();
        ids.sort_unstable();
        for id in ids {
            let s = self.slots[self.find(id).unwrap()];
            hash_u(&mut h, id);
            h = (h ^ (s.order.side as u8 as u64)).wrapping_mul(1_099_511_628_211);
            hash_u(&mut h, s.order.price);
            hash_u(&mut h, s.order.quantity)
        }
        h
    }
    fn find(&self, id: OrderId) -> Option<usize> {
        let m = self.slots.len() - 1;
        let mut i = mix(id) as usize & m;
        for _ in 0..self.slots.len() {
            let s = self.slots[i];
            if s.state == State::Empty {
                return None;
            }
            if s.state == State::Occupied && s.id == id {
                return Some(i);
            }
            i = (i + 1) & m
        }
        None
    }
    fn insertion(&self, id: OrderId) -> Result<usize, &'static str> {
        let m = self.slots.len() - 1;
        let (mut i, mut tomb) = (mix(id) as usize & m, None);
        for _ in 0..self.slots.len() {
            let s = self.slots[i];
            if s.state == State::Empty {
                return Ok(tomb.unwrap_or(i));
            }
            if s.state == State::Tombstone && tomb.is_none() {
                tomb = Some(i)
            }
            if s.state == State::Occupied && s.id == id {
                return Err("index");
            }
            i = (i + 1) & m
        }
        tomb.ok_or("index")
    }
    fn offset(&self, p: Price) -> Result<usize, &'static str> {
        if p < self.minimum {
            return Err("price");
        }
        let i = (p - self.minimum) as usize;
        if i >= self.bids.len() { Err("price") } else { Ok(i) }
    }
    fn add(&mut self, id: OrderId, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        if id == 0 || q == 0 {
            return Err("add");
        }
        let i = self.insertion(id)?;
        self.add_level(side, p, q)?;
        self.slots[i] =
            Slot { id, order: Order { side, price: p, quantity: q }, state: State::Occupied };
        self.live += 1;
        Ok(())
    }
    fn remove(&mut self, id: OrderId) -> Result<(), &'static str> {
        let i = self.find(id).ok_or("id")?;
        let o = self.slots[i].order;
        self.sub_level(o.side, o.price, o.quantity)?;
        self.slots[i].state = State::Tombstone;
        self.live -= 1;
        Ok(())
    }
    fn reduce(&mut self, id: OrderId, q: Quantity, full: bool) -> Result<(), &'static str> {
        let i = self.find(id).ok_or("id")?;
        let o = self.slots[i].order;
        if q == 0 || q > o.quantity || (!full && q == o.quantity) {
            return Err("reduce");
        }
        self.sub_level(o.side, o.price, q)?;
        if q == o.quantity {
            self.slots[i].state = State::Tombstone;
            self.live -= 1
        } else {
            self.slots[i].order.quantity -= q
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
        if let Some(i) = self.find(id) {
            let o = self.slots[i].order;
            self.sub_level(o.side, o.price, o.quantity)?;
            self.add_level(side, p, q)?;
            self.slots[i].order = Order { side, price: p, quantity: q };
            Ok(())
        } else {
            self.add(id, side, p, q)
        }
    }
    fn add_level(&mut self, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        let i = self.offset(p)?;
        let l = if side == Side::Bid { &mut self.bids } else { &mut self.asks };
        l[i] = l[i].checked_add(q).ok_or("overflow")?;
        if side == Side::Bid {
            if self.best_bid.map_or(true, |x| p > x) {
                self.best_bid = Some(p)
            }
        } else if self.best_ask.map_or(true, |x| p < x) {
            self.best_ask = Some(p)
        }
        Ok(())
    }
    fn sub_level(&mut self, side: Side, p: Price, q: Quantity) -> Result<(), &'static str> {
        let i = self.offset(p)?;
        let l = if side == Side::Bid { &mut self.bids } else { &mut self.asks };
        if q == 0 || q > l[i] {
            return Err("level");
        }
        l[i] -= q;
        if l[i] == 0
            && ((side == Side::Bid && self.best_bid == Some(p))
                || (side == Side::Ask && self.best_ask == Some(p)))
        {
            self.refresh(side)
        }
        Ok(())
    }
    fn refresh(&mut self, side: Side) {
        let l = if side == Side::Bid { &self.bids } else { &self.asks };
        let found = if side == Side::Bid {
            l.iter().rposition(|&q| q != 0)
        } else {
            l.iter().position(|&q| q != 0)
        };
        if side == Side::Bid {
            self.best_bid = found.map(|i| self.minimum + i as u64)
        } else {
            self.best_ask = found.map(|i| self.minimum + i as u64)
        }
    }
}
fn capacity(n: usize) -> usize {
    (n.saturating_mul(2)).max(8).next_power_of_two()
}
fn mix(mut x: u64) -> u64 {
    x = x.wrapping_add(0x9E37_79B9_7F4A_7C15);
    x = (x ^ (x >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    x = (x ^ (x >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    x ^ (x >> 31)
}
fn hash_u(h: &mut u64, x: u64) {
    for s in (0..64).step_by(8) {
        *h = (*h ^ ((x >> s) as u8 as u64)).wrapping_mul(1_099_511_628_211)
    }
}
