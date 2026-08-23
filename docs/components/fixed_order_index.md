# Fixed order-ID index: design note

## Why it exists

An L3 order book must locate an order by its venue order ID for cancel, execute,
delete, replace, and update. This lookup sits directly on the critical update
path. The previous experimental index used tombstones after delete: correct,
but its probe chains can grow with the *history* of the book rather than its
current live load.

`FixedOrderIndex<Value>` is a bounded reusable alternative. It is not yet the
active order-book index; integration waits for its independent benchmark.

## Chosen representation

The table is a fixed vector of slots `{order_id, value, occupied}`. Capacity
is a power of two and at least twice the configured maximum number of live
entries. `SplitMix64(order_id) & mask` chooses a home slot; collisions probe
forward one slot at a time.

There is no allocation or resize after construction. `order_id == 0` is
reserved as invalid input; this makes an unused slot unambiguous.

## The key change: deletion

After removing a slot, an empty hole would make later entries in the same
probe cluster unreachable: lookup stops at the first empty slot. Instead of
leaving a tombstone forever, `erase_at` walks forward until the end of that
cluster. It moves an entry back into the hole only when the hole lies on that
entry's circular path from its home slot. The final hole becomes empty.

This preserves lookup correctness without historical tombstones.

## Cost and trade-off

- Insert/find remain expected O(1) at the bounded load factor.
- Erase may move several adjacent entries, so it has a variable cost.
- The trade is intentional: bounded work during an erase versus potentially
  unbounded historical degradation on every later lookup/insert.

The next benchmark must measure both sides under controlled collision and
churn profiles; this document is a design rationale, not a performance claim.

## Where to read the code

- C++: [`cpp/include/llab/fixed_order_index.hpp`](../../cpp/include/llab/fixed_order_index.hpp)
- Rust: [`rust/src/fixed_order_index.rs`](../../rust/src/fixed_order_index.rs)
- Matching model and collision tests: [`cpp/tests/fixed_order_index.cpp`](../../cpp/tests/fixed_order_index.cpp) and [`rust/tests/fixed_order_index.rs`](../../rust/tests/fixed_order_index.rs)
