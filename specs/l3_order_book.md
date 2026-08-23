# L3 order-book state engine — experiment 01

## Purpose and boundary

This experiment measures the in-memory state-update cost of a single-symbol, single-threaded L3 order-book engine. It compares equivalent C++ and Rust implementations before comparing independently designed reference implementations.

The timed component accepts already-decoded canonical events and maintains individual displayed orders, aggregated price-level quantities, and best bid/ask. It deliberately excludes packet reception, wire decoding, sequencing, gap recovery, snapshots, multi-symbol dispatch, persistence, strategy logic, matching, and metrics export. Those are separate experiments.

The full-lifecycle event model follows order-by-order protocols such as Nasdaq TotalView-ITCH: add, execute, cancel, delete, and replace. It is a canonical internal model, **not** a byte-compatible feed parser. Some public order-level sources disclose only changes to an individual order's state; their source-neutral upsert/delete events are also supported and never assigned an invented business cause.

## Research questions

1. With the same event trace and book contract, how do matched C++ and Rust implementations behave in throughput, latency distribution, generated code, and hardware-counter measurements?
2. When each language is permitted a different practical internal design, which design is preferable for a stated workload and why?
3. What changes in the conclusion when the update distribution, active-order count, price range, or burst pattern changes?

No implementation is called "best" without a stated workload, environment, constraints, measurements, and explanation of non-equivalent choices.

## Canonical state

The engine represents one instrument with a fixed positive tick size. Prices are unsigned integer tick indices; quantities are positive unsigned integers; order identifiers are non-zero unsigned 64-bit values.

For every live order, the state contains:

- `order_id`
- `side`: `Bid` or `Ask`
- `price_ticks`
- `remaining_quantity`

For every occupied `(side, price_ticks)` level, the state contains the sum of remaining quantities of its live orders. The engine exposes:

- best bid: highest occupied bid price and its aggregate quantity;
- best ask: lowest occupied ask price and its aggregate quantity;
- the aggregate quantity at a queried price level;
- live-order count.

The engine does not model queue position, participant attribution, hidden/iceberg liquidity, auction rules, crossed-book prevention, or matching. FIFO order within a price level is therefore not part of this experiment's contract.

## Canonical events

Every input trace is valid: an event referring to an order identifier always refers to a live order, and every quantity is valid for that order at that point in the trace.

| Event | Fields | Required transition |
| --- | --- | --- |
| `Add` | `order_id`, `side`, `price_ticks`, `quantity` | Insert a previously unused identifier and add its quantity to its level. |
| `Cancel` | `order_id`, `quantity` | Subtract quantity from the order and level. The order remains live; cancel quantity is strictly less than remaining quantity. |
| `Execute` | `order_id`, `quantity` | Subtract quantity from the order and level. If it reaches zero, remove the order and remove an empty level. |
| `Delete` | `order_id` | Remove the remaining quantity, order, and any newly empty level. |
| `Replace` | `old_order_id`, `new_order_id`, `side`, `price_ticks`, `quantity` | Remove the old order exactly as `Delete`, then add a new order exactly as `Add`. The new identifier is unused. |
| `OrderUpsert` | `order_id`, `side`, `price_ticks`, `remaining_quantity` | Insert the order if absent; otherwise replace its complete displayed state and adjust the affected aggregate level(s). This event records a source-observable state change, not its cause. |
| `OrderDelete` | `order_id` | Remove the order exactly as `Delete`, when a source reports only removal and does not disclose whether it was canceled or executed. |

`Cancel` intentionally excludes full cancellation; full removal is represented by `Delete`. This keeps lifecycle interpretation unambiguous and close to distinct cancel/delete messages in common order-by-order feeds. `OrderUpsert` and `OrderDelete` are not substitutes for lifecycle data: they exist for a separate source family that does not expose such causes.

All quantities and aggregated level values must remain representable in `u64`. A trace that would overflow is invalid and must be rejected by the generator, not allowed into a benchmark.

## Correctness contract

After every event, an implementation must preserve these invariants:

1. Each live identifier maps to exactly one live order.
2. The quantity at each level equals the sum of its live orders' remaining quantities.
3. No live order or occupied level has zero quantity.
4. Best bid and best ask agree with the highest/lowest occupied price on their respective sides.
5. Live-order count equals the number of live identifiers.

Each trace ends with a deterministic state digest. The digest covers live-order count, best bid/ask, all non-empty price levels in canonical order, and all live orders in ascending identifier order. C++ and Rust must produce the same digest. Tests also validate selected intermediate checkpoints, so a matching final state cannot hide an earlier divergent transition.

## Input traces and source profiles

The initial synthetic traces are deterministic. They are structurally shaped like an order-by-order feed: identifiers have lifecycles, partial cancels/executions occur, levels become empty, and bursty periods exist. They are **not** claimed to reproduce the statistical distribution of any market until calibrated against a named source and capture period.

Each trace records its seed, configuration, event count, initial book, event mix, active-order-count distribution, occupied-price range, and burst schedule. Trace generation and checksum occur outside the timed region.

The first trace suite will contain:

- `steady`: mixed lifecycle updates with a bounded active book;
- `cancel_heavy`: high cancel/replace pressure and frequent level removal;
- `execution_heavy`: partial and complete executions against existing orders;
- `wide_sparse`: a broad tick range with sparse occupied levels;
- `burst`: a normal baseline interrupted by short high-rate update intervals.

The suite is a coverage matrix, not a realism claim. Its parameters remain explicitly provisional until a later calibration experiment measures distributions from a named venue, instrument class, session window, and data source.

### Nasdaq TotalView-ITCH sample profile

Official Nasdaq sample files provide a deterministic, full-lifecycle reference profile. They are normalized into `Add`, `Cancel`, `Execute`, `Delete`, and `Replace` events. This source validates lifecycle semantics, replay, and correctness fixtures; its public sample status does not make it a claim about current market distributions. Provenance and scope are recorded in [`docs/adr/0001-l3-data-sources.md`](../docs/adr/0001-l3-data-sources.md).

### Bitfinex Raw Books profile

The public Bitfinex `R0` stream provides live order identifiers with snapshot/upsert/delete state changes. It is normalized only into `OrderUpsert` and `OrderDelete`; the adapter must not label a change as cancel or execution. Its bounded visible raw-book view is useful for a no-registration live crypto update profile, but it is not presented as complete market depth or full lifecycle data. The source contract is documented in [`docs/data-sources/bitfinex_raw_books.md`](../docs/data-sources/bitfinex_raw_books.md).

## Comparison stages

### Stage A — semantic parity

Both languages use the same canonical events, trace, digest, timing boundary, and externally visible contract. The initial design choices are intentionally conservative and are documented before coding. Equivalent does not mean source-code similarity; it means equivalent observable behavior and comparable constraints.

### Stage B — reference implementations

Each implementation may choose a different representation, allocation policy, index, price-level structure, or safe/unsafe boundary. Every difference is recorded in an implementation note with its rationale, expected benefit, downside, and validation evidence.

Candidate structures are hypotheses rather than defaults:

- a dense tick-indexed price ladder may benefit a narrow, bounded range but waste memory or cache capacity for a sparse range;
- an ordered price index can make best-price discovery straightforward but introduces pointer/cache and update costs;
- an identifier index may trade memory for expected lookup cost;
- preallocation may reduce allocator work while increasing memory footprint and capacity assumptions.

## Measurement boundary and reporting

One sample measures application of exactly one pre-generated trace to a freshly prepared engine state. Trace construction, allocation policy setup, digest verification, output, and sample aggregation occur outside the timer interval unless a later experiment explicitly puts them inside scope.

For every result, report at minimum:

- trace identity and configuration;
- event count and operation mix;
- warm-up and sample protocol;
- total events per second and nanoseconds per event;
- min, p50, p90, p95, p99, p99.9, max, and mean sample time;
- host CPU, memory, OS/kernel, CPU-frequency policy, affinity/isolation policy, compiler version/flags, and build profile;
- correctness digest and whether generated assembly and hardware counters were inspected.

The initial wall-clock measurements are methodology baselines. Claims about low-latency suitability require later controlled-host measurements and a separately reported measurement-system perturbation budget.

## Open decisions before implementation

- Exact initial trace length, warm-up count, and sample count.
- Initial active-order capacity and allowed price range for each trace.
- The canonical serialization format for generated traces and expected digests.
- The parity-stage internal representation.
- The independent reference designs to evaluate after parity is verified.
