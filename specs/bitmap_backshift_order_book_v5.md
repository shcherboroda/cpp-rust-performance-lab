# Bitmap backshift order book v5

## Purpose

v5 is a separate control-preserving variant of bitmap v4. It retains v4's
256-price dense ladders and occupancy bitmap, but replaces only its order-ID
index deletion policy: tombstones become backward-shift deletion.

## Non-negotiable parity

- Same `Event` and public order-book contract as v4.
- Same maximum live order bound, power-of-two capacity and SplitMix64 mixer.
- The index value is the compact `(side, price, quantity)` order record only;
  it is not a heap-owned payload or an intrusive node.
- Same LLBT fixture state/digest, update-only trace and update+BBO trace.
- No allocation after construction.

## Expected trade-off

Removing an order may move following compact entries in its collision cluster.
In return, future probes do not retain tombstone history. The isolated index
result is a hypothesis generator, not evidence that v5 improves full-book
latency. v5 is accepted only after the common matrix, resource audit and state
tests are run against both v4 and v5.
