# Bybit V5 public order-book L2 pipeline v1

## Purpose

Bybit V5 public order-book is the primary live L2 source. It is anonymous,
provides a snapshot followed by price-level deltas, and is deliberately a
separate experiment from an order-by-order L3 book. No live timing is used as
a language benchmark; captures are frozen before offline comparison.

## Official source contract

Source: <https://bybit-exchange.github.io/docs/v5/websocket/public/orderbook>
(retrieved 2026-09-11).

- Subscribe to one public `orderbook.{depth}.{symbol}` topic. Initial work uses
  `orderbook.50.BTCUSDT` on the appropriate public V5 endpoint.
- The first payload is a `snapshot`; its bid and ask arrays replace the local
  book in full. A subsequent `snapshot`, including one with `u == 1` after a
  service restart, also replaces the book in full.
- A `delta` contains `[price, size]` entries for each side. `size == 0`
  removes the price level; otherwise it inserts or replaces that level's
  aggregate quantity.
- `u` is the per-book update ID. The adapter accepts a delta only when it is
  exactly one greater than the accepted update ID. Any other delta invalidates
  the local book until a new snapshot arrives. This is intentionally stricter
  than assuming TCP prevents loss.
- `seq` is a cross-sequence for ordering different depths, not the local replay
  gate. It is captured for provenance/telemetry but does not replace `u`.
- Retail Price Improvement orders are excluded by the public stream. The book
  is therefore a venue-provided visible L2 view, not a claim of every order.

## Canonical L2 model

The adapter converts decimal price and size to unsigned 1e-8 integers. It
emits `Snapshot(update_id, bids, asks)` and `Delta(update_id, bid_changes,
ask_changes)`. The book stores only aggregate price levels, exposes BBO and
level quantities, and has no order IDs or lifecycle causes.

The parity baseline is two sorted, contiguous vectors (bids descending, asks
ascending). For the initial depth-50 stream this makes BBO a direct first-level
read and keeps state/digest ordering deterministic. It does **not** re-sort on
each delta: it binary-searches the insertion point and moves at most 49 compact
levels. A delta insert/delete is still O(N), so this is a hypothesis, not an
assumed final representation.

### Representation decision protocol

Before selecting an optimized book, compare the same pre-normalized traces and
the same BBO/digest oracle across these candidates:

| Candidate | Expected strength | Expected cost / disqualifier |
| --- | --- | --- |
| Sorted fixed-capacity vector | Depth 50; replace-heavy updates; cache locality; direct BBO. | O(N) shifts on insert/delete; capacity must be explicit. |
| Dense price ladder | Narrow bounded tick range; high update rate; O(1) level update. | Memory/range cost, clearing/rebase policy, sparse-book cache waste. |
| Ordered tree/map | Wide and sparse prices; unbounded level count. | Pointer chasing, allocator behavior, and less comparable ownership/layout. |

The first benchmark suite contains four frozen workloads: (1) top-of-book
churn, (2) interior insert/delete churn, (3) replace-heavy steady state, and
(4) wide sparse depth. Each is run at depth 50 and, separately, depth 1000.
Every candidate receives pre-reserved/preallocated state before timing. Timed
work is book application only; parsing, fixture construction, allocation,
digest validation and reporting are outside the interval. Record 10 warm-ups,
200 retained samples, raw CSV, p50/p95/p99/p99.5, mean, max, live-level count and
price-span distribution. A representation changes only after the matching
fixture tests and this matrix support the change.

The checked-in fixture covers snapshot, insert, replace, delete, a valid
sequence suffix, service-restart resnapshot, and a rejected update-ID gap.
Both C++ and Rust must produce the same final BBO and level-state digest.

## Implemented C++ vertical slice

`bybit_l2_capture` provides the first operational C++ path:

```text
Bybit V5 TLS WebSocket -> exact LLFR raw capture -> Bybit JSON adapter
  -> snapshot/u gate -> sorted-vector L2 book -> BBO/status
                      ^
                   LLFR replay
```

`--record PATH` persists both the outbound subscription and every inbound text
frame before decoding. The writer uses a bounded SPSC handoff to isolate file
I/O. Queue overflow, an over-large frame or writer failure terminates the run;
it is never silently treated as a valid capture. `--replay PATH` applies the
same adapter and book gate to saved records without a network connection.
The current JSON adapter is correctness-first and allocating; it is deliberately
outside book-only benchmarks and is a replacement point for a bounded faster
parser.

## Boundaries

The implemented capture path is raw frames -> bounded queue -> persistent
capture and Bybit parser -> L2 sequence/snapshot gate -> L2 book. File I/O,
capture, allocation/reservation, final digest validation, and reporting remain
outside offline book-application timed regions.
