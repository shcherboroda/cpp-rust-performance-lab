use std::collections::BTreeMap;
use crate::bybit_l2_order_book::{Change, Level, Price, Quantity, Side};
pub struct OrderedMapBook { bids:BTreeMap<Price,Quantity>, asks:BTreeMap<Price,Quantity>, update_id:u64, valid:bool }
impl OrderedMapBook {
 pub fn new()->Self{Self{bids:BTreeMap::new(),asks:BTreeMap::new(),update_id:0,valid:false}}
 pub fn apply_snapshot(&mut self,u:u64,b:&[Level],a:&[Level])->bool{self.bids.clear();self.asks.clear();if u==0||!self.load(b,Side::Bid)||!self.load(a,Side::Ask){self.valid=false;return false}self.update_id=u;self.valid=true;true}
 pub fn apply_delta(&mut self,u:u64,c:&[Change])->bool{if !self.valid||u!=self.update_id+1{self.valid=false;return false}for x in c{let m=if x.side==Side::Bid{&mut self.bids}else{&mut self.asks};if x.quantity==0{m.remove(&x.price);}else{m.insert(x.price,x.quantity);}}self.update_id=u;true}
 pub fn best_bid(&self)->Option<Level>{self.bids.last_key_value().map(|(p,q)|Level{price:*p,quantity:*q})}
 pub fn best_ask(&self)->Option<Level>{self.asks.first_key_value().map(|(p,q)|Level{price:*p,quantity:*q})}
 fn load(&mut self,l:&[Level],s:Side)->bool{for x in l{if x.quantity==0{return false}(if s==Side::Bid{&mut self.bids}else{&mut self.asks}).insert(x.price,x.quantity);}true}
}
