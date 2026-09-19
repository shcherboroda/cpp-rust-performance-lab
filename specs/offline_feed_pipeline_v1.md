# Offline Bitfinex R0 feed pipeline v1

## Purpose

Build a reproducible, offline comparison boundary from Bitfinex Raw Books R0
application frames to a validated L3 book state. It joins existing raw-frame,
decoder, and book components without treating live Internet timing as a
language benchmark.

The pipeline answers two distinct questions:

1. How do matched C++ and Rust implementations perform for the same frozen
   raw-frame replay and canonical event contract?
2. After the matched baseline, what do practical ecosystem parsers and
   independently optimized decoders cost in each language?

Neither question ranks C++ and Rust in general.

## Source and fixture

- Endpoint and subscription: Bitfinex `wss://api-pub.bitfinex.com/ws/2`,
  `tBTCUSD`, `book`, `prec=R0`, `freq=F0`, `len=25`.
- The capture records exact text-frame bytes before parsing, using the existing
  `RawFrameRecord` binary encoding and SHA-256 manifest described in
  [`market_data_capture_v1.md`](market_data_capture_v1.md).
- A frozen capture segment is the benchmark input. Its manifest digest,
  frame count, byte count, connection epoch, subscription payload and decoder
  version are reported with every result.
- A small checked-in derived fixture covers control, subscription acknowledgement,
  snapshot, valid upsert, delete, malformed payload, wrong channel, and a new
  connection epoch. A larger raw capture remains local and Git-ignored.

## Source semantics

After the `subscribed` acknowledgement identifies the dynamic channel ID:

- a snapshot is `[channel_id, [[order_id, price, amount], ...]]`;
- an update is `[channel_id, [order_id, price, amount]]`;
- non-zero price normalizes to `OrderUpsert`;
- zero price normalizes to `OrderDelete`;
- positive amount is `Bid`; negative amount is `Ask`;
- the quantity is the absolute fixed-point amount.

Price and quantity use a shared decimal-to-integer scale defined by the
fixture manifest. Conversion rejects excess precision, zero quantity for a
non-delete update, non-finite values, integer overflow, and a zero price in an
upsert. The adapter never invents `Cancel` or `Execute` semantics.

Bitfinex R0 does **not** expose a monotonic order-book sequence. Therefore
`FeedSequenceRecovery` is not part of this v1 timed pipeline and must not be
fed synthetic sequence numbers. A connection epoch, subscription/channel gate,
and snapshot-complete state guard book validity instead. The existing sequence
recovery experiment remains applicable to a future sequenced source such as
Nasdaq ITCH.

## Pipeline contract

```text
frozen RawFrameRecord stream
  -> validate record CRC and text direction
  -> bounded SPSC queue
  -> R0 frame classifier and parser
  -> channel/session/snapshot gate
  -> canonical OrderUpsert / OrderDelete
  -> parity L3 book v2
  -> final state digest and counters
```

The primary v1 book is the fixed-layout parity implementation. Both languages
must apply the same canonical events and produce the same final book digest,
live-order count, BBO, frame-class counters, reject counters, and connection
epoch count. The optimized-reference track may substitute a later book version,
but its result is reported separately.

Failure to decode a data frame, a channel mismatch after subscription, queue
overflow, record corruption, or disconnect invalidates the current book. The
next connection epoch requires a fresh snapshot before any update is applied.
Control frames, heartbeats, acknowledgements, and invalid frames are counted
but do not become book events.

## Implementations

### A. Matched parser baseline

Both implementations use an explicit bounded parser for the restricted R0
grammar. They scan UTF-8 bytes, parse only the documented JSON array shapes,
and convert numbers with the shared fixed-point rule. They allocate no storage
after construction; event/output buffers, queue slots, and book capacity are
reserved before the timed region.

### B. Ecosystem parser baseline

C++ and Rust may use practical general-purpose JSON libraries, respectively,
but this is labelled an ecosystem-library comparison, not a standard-library
comparison. It shares the exact raw fixture and normalization/oracle with A.
Parser DOM allocation and conversion are included because they are part of the
chosen approach.

### C. Optimized references

Each language may use an independently justified decoder and book version. The
report identifies every difference in data layout, ownership, parser strategy,
allocation behavior, validation, and error representation. No result from C
is presented as a language-only result.

## Timed boundaries

Each boundary has a separate executable and raw CSV output:

| ID | Timed work | Excludes |
| --- | --- | --- |
| `r0_decode_normalize` | Frame bytes to canonical events and counters. | Queue, book application, fixture verification. |
| `r0_queue_decode` | Producer handoff plus consumer decode/normalize. | File read, construction, book application. |
| `r0_replay_book` | Canonical events through the parity book. | Raw parsing and queue. |
| `r0_end_to_end_offline` | Preloaded records through queue, parser, gate and book. | File I/O, allocation/reserve, final validation and reporting. |

The end-to-end benchmark reports frames/s, canonical events/s, elapsed time per
frame/event, p50/p95/p99 per sample, and per-process percentiles. It is a
batch-throughput boundary, not a claim about the latency of one live market
update. Runs preserve 15 alternating process pairs, 10 warm-ups, 200 retained
samples, CPU affinity, compiler versions, power profile, fixture digest, and
binary hashes.

## Acceptance criteria

1. C++ and Rust pass the same derived-frame correctness fixture.
2. A frozen raw segment produces identical canonical-event digest and final
   parity-book digest in both languages.
3. Invalid and epoch-transition cases leave the book invalid until a fresh
   snapshot is accepted.
4. Allocation audits show no steady-state allocation for A and the parity
   end-to-end path.
5. The first native-Linux result includes raw samples, host metadata, and a
   parity audit before any optimization conclusion.
