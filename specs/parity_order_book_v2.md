# Parity order-book v2

## Purpose

This variant removes standard-container differences from the main state-update path. It is a separate implementation and benchmark; it does not replace the earlier native-standard-library baseline.

## Identical algorithm contract

- Order IDs are stored in a fixed-capacity, open-addressed table.
- Capacity is the smallest power of two not less than `2 * maximum_live_orders` (minimum 8).
- Initial slot is `SplitMix64(order_id) & (capacity - 1)`; collisions use linear probing.
- Deleted slots become tombstones. Insertion reuses the first tombstone encountered, while lookup continues until an empty slot.
- The table never grows during an experiment. A full table is a contract violation.
- Bid and ask levels are independently maintained as contiguous arrays sorted by ascending price. Lookup uses binary search; inserting/removing a level shifts the contiguous tail.
- Best bid is the last bid level; best ask is the first ask level.

The event representation stores four `u64` fields followed by one-byte type and side tags; its required size is 40 bytes in both C++ and Rust. Benchmark startup fails if the size differs.

## Scope

This is a semantic-parity and data-structure-parity implementation, not a claim that this is the ideal design for every market. Its fixed capacity and vector level structure deliberately trade generality for predictable layout. Later reference variants may change one property at a time, with the same change first applied to both languages.
