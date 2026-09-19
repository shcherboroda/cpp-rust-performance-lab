use crate::bybit_l2_order_book::{Change, Level, Price, Quantity, Side};
pub struct DenseLadderBook { minimum: Price, bids: Vec<Quantity>, asks: Vec<Quantity>, update_id:u64, valid:bool }
impl DenseLadderBook {
 pub fn new(minimum:Price, price_count:usize)->Self { Self{minimum,bids:vec![0;price_count],asks:vec![0;price_count],update_id:0,valid:false} }
 pub fn apply_snapshot(&mut self,u:u64,b:&[Level],a:&[Level])->bool { self.bids.fill(0);self.asks.fill(0); if u==0||!self.load(b,Side::Bid)||!self.load(a,Side::Ask){self.valid=false;return false} self.update_id=u;self.valid=true;true }
 pub fn apply_delta(&mut self,u:u64,c:&[Change])->bool { if !self.valid||u!=self.update_id+1 {self.valid=false;return false} for x in c {if !self.set(*x){self.valid=false;return false}} self.update_id=u;true }
 pub fn best_bid(&self)->Option<Level>{self.bids.iter().rposition(|q|*q>0).map(|i|Level{price:self.minimum+i as u64,quantity:self.bids[i]})}
 pub fn best_ask(&self)->Option<Level>{self.asks.iter().position(|q|*q>0).map(|i|Level{price:self.minimum+i as u64,quantity:self.asks[i]})}
 fn set(&mut self,c:Change)->bool { let Some(i)=c.price.checked_sub(self.minimum).and_then(|n|usize::try_from(n).ok()).filter(|i|*i<self.bids.len()) else{return false}; if c.side==Side::Bid{self.bids[i]=c.quantity}else{self.asks[i]=c.quantity};true }
 fn load(&mut self,l:&[Level],s:Side)->bool {for x in l {if x.quantity==0||!self.set(Change{side:s,price:x.price,quantity:x.quantity}){return false}}true}
}
