# LLBT trace format v1

LLBT (*Low-Latency Lab Book Trace*) is a compact, deterministic text fixture format for cross-language correctness tests. It is not a production capture format and is never parsed inside a timed benchmark region.

## File rules

- The first non-comment line is exactly `LLBT/1`.
- Blank lines and lines beginning with `#` are ignored.
- Fields are separated by one or more ASCII spaces.
- Identifiers, price ticks, and quantities are unsigned base-10 64-bit integers.
- `B` means bid; `A` means ask.
- All events must satisfy the validity contract in [`l3_order_book.md`](l3_order_book.md).

## Events

| Record | Fields | Canonical event |
| --- | --- | --- |
| `A` | `order_id side price_ticks quantity` | `Add` |
| `C` | `order_id quantity` | `Cancel` |
| `E` | `order_id quantity` | `Execute` |
| `D` | `order_id` | `Delete` |
| `R` | `old_order_id new_order_id side price_ticks quantity` | `Replace` |
| `U` | `order_id side price_ticks remaining_quantity` | `OrderUpsert` |
| `X` | `order_id` | `OrderDelete` |

The final record is `EXPECT live_order_count best_bid_price best_bid_quantity best_ask_price best_ask_quantity state_digest`. An absent best side is encoded as price `0`, quantity `0`.

`state_digest` is an unsigned decimal FNV-1a 64-bit value. Hash, in order: live-order count; bid-level count and ascending `(price, quantity)` pairs; ask-level count and ascending pairs; live orders sorted by ascending ID as `(id, side_byte, price, remaining_quantity)`. Each integer is encoded as eight little-endian bytes; `Bid` has side byte `0`, `Ask` has side byte `1`; FNV offset basis is `14695981039346656037` and prime is `1099511628211` with wrapping `u64` multiplication.

`LLBT/1` supports only one symbol. Source adapters must convert decimal quantities/prices and any multi-symbol stream outside this format according to their own documented policies.
