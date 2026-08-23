# Feed sequence recovery v1

## Purpose

`FeedSequenceRecovery` is a bounded, single-writer control component placed
between a venue adapter and an order book. It prevents an L3 book from
silently applying a discontinuous incremental stream.

It does not parse venue payloads, own the order book, allocate in its steady
state, or claim that every venue exposes the same sequence semantics. An
adapter normalizes its venue-specific protocol into this contract.

## Contract

- In `Live`, an incremental event with `sequence == expected` is emitted for
  application and advances `expected`.
- An event below `expected` is stale/duplicate and is ignored.
- An event above `expected` is a gap: no event is emitted, state becomes
  `AwaitingSnapshot`, and the caller must request recovery.
- `begin_snapshot(last_sequence)` enters `Snapshotting` and clears any old
  replay buffer. Snapshot book mutations are applied by the caller directly.
- Incrementals received during `Snapshotting` are retained only when their
  sequence is greater than `last_sequence`; capacity is fixed at construction.
  Overflow is a recovery failure, not silent data loss.
- `finish_snapshot()` sorts the bounded replay buffer by sequence, emits only
  the contiguous suffix starting from `last_sequence + 1`, then enters `Live`.
  Any replay gap returns to `AwaitingSnapshot` without emitting a partial,
  potentially inconsistent suffix.

The caller must serialize calls. Multi-reader publication is a later component.
