# Packed order record v6

v6 tests only order-record layout inside the bounded ID index. It keeps the
256-price ladder contract and `u64` quantity. `side` is stored in the high bit
of a packed price-offset word; the remaining bits store the 0..255 offset from
the configured minimum price. The record is therefore two `u64` words:
`packed_side_offset` and `quantity` (16 bytes), rather than an aligned
`{side, absolute_price, quantity}` record (typically 24 bytes).

The packed representation must reject an out-of-range price before mutation,
round-trip side/price/quantity through the shared state digest, retain zero
update allocations, and be benchmarked against the same v4 control and v5
backshift variant. No narrowing of quantity is allowed in v6.
