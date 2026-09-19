pub type Price = u64;
pub type Quantity = u64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Side { Bid, Ask }
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Level { pub price: Price, pub quantity: Quantity }
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Change { pub side: Side, pub price: Price, pub quantity: Quantity }

pub struct OrderBook { bids: Vec<Level>, asks: Vec<Level>, maximum_levels: usize, update_id: u64, valid: bool }
impl OrderBook {
    pub fn new(maximum_levels: usize) -> Self { Self { bids: Vec::with_capacity(maximum_levels), asks: Vec::with_capacity(maximum_levels), maximum_levels, update_id: 0, valid: false } }
    pub fn apply_snapshot(&mut self, update_id: u64, bids: &[Level], asks: &[Level]) -> bool {
        if update_id == 0 || bids.len() > self.maximum_levels || asks.len() > self.maximum_levels || !valid_levels(bids, Side::Bid) || !valid_levels(asks, Side::Ask) { return false; }
        self.bids = bids.to_vec(); self.asks = asks.to_vec(); self.update_id = update_id; self.valid = true; true
    }
    pub fn apply_delta(&mut self, update_id: u64, changes: &[Change]) -> bool {
        if !self.valid || update_id != self.update_id + 1 { self.valid = false; return false; }
        for change in changes { if !self.apply_change(*change) { self.valid = false; return false; } }
        self.update_id = update_id; true
    }
    pub fn valid(&self) -> bool { self.valid }
    pub fn best_bid(&self) -> Option<Level> { self.bids.first().copied() }
    pub fn best_ask(&self) -> Option<Level> { self.asks.first().copied() }
    pub fn level_count(&self) -> usize { self.bids.len() + self.asks.len() }
    pub fn state_digest(&self) -> u64 { let mut h = 14_695_981_039_346_656_037; for levels in [&self.bids, &self.asks] { hash(&mut h, levels.len() as u64); for l in levels { hash(&mut h,l.price); hash(&mut h,l.quantity); } } h }
    fn apply_change(&mut self, change: Change) -> bool {
        let levels = if change.side == Side::Bid { &mut self.bids } else { &mut self.asks };
        let index = levels.partition_point(|level| if change.side == Side::Bid { level.price > change.price } else { level.price < change.price });
        if index < levels.len() && levels[index].price == change.price { if change.quantity == 0 { levels.remove(index); } else { levels[index].quantity = change.quantity; } } else if change.quantity != 0 { if levels.len() == self.maximum_levels { return false; } levels.insert(index, Level { price: change.price, quantity: change.quantity }); }
        true
    }
}
fn valid_levels(levels: &[Level], side: Side) -> bool { levels.iter().enumerate().all(|(i, l)| l.quantity != 0 && (i == 0 || if side == Side::Bid { levels[i-1].price > l.price } else { levels[i-1].price < l.price })) }
fn hash(hash: &mut u64, mut value: u64) { for _ in 0..8 { *hash ^= value & 0xff; *hash = hash.wrapping_mul(1_099_511_628_211); value >>= 8; } }
