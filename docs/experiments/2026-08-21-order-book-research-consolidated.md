# L3 order-book research — consolidated WSL2 baseline

## Scope and evidence standard

This report covers one deterministic synthetic L3 component workload, not an
exchange gateway, matching engine, market-data parser, NIC path or complete HFT
system. Results are WSL2 desktop baselines with process alternation and many
samples; they are useful for falsifying simple claims and choosing next tests,
but are not native-Linux tail-latency or language-wide claims.

## What was built

| Component | C++ | Rust | Validation |
| --- | --- | --- | --- |
| L3 book v1–v4 | yes | yes | shared LLBT fixture and state digest |
| Common mixed lifecycle matrix | yes | yes | 15 alternating processes × 200 samples |
| Fixed backshift order-ID index | yes | yes | collision, long-history, randomized model tests |
| Feed sequence/snapshot/replay state | yes | yes | gap, duplicate, replay and overflow tests |
| Bitmap allocation audit | yes | yes | 0 allocation calls after construction |
| Single-writer BBO snapshot cell | yes | yes | concurrent cross-field-invariant stress test |

## Primary L3 matrix

The common stream applies 57,344 valid events: add, partial cancel, upsert,
replace, full execute and delete/order-delete. It is measured both as
`update-only` and `update+BBO` (read bid and ask after every event).

| Version / boundary | C++ median process p50 | Rust median process p50 |
| --- | ---: | ---: |
| v1 native, update-only | 1.992 ms | 1.892 ms |
| v2 parity, update-only | 1.219 ms | 1.206 ms |
| v3 dense cached-BBO, update-only | 5.814 ms | 5.518 ms |
| v4 bitmap, update-only | 1.084 ms | 1.130 ms |
| v1 native, update+BBO | 3.889 ms | 3.594 ms |
| v2 parity, update+BBO | 1.451 ms | 1.189 ms |
| v3 dense cached-BBO, update+BBO | 6.334 ms | 6.238 ms |
| v4 bitmap, update+BBO | 1.753 ms | 1.391 ms |

The v3/v4 difference is a placement-of-work trade-off. v3 scans the dense
ladder immediately when a best level empties, even if no reader asks for BBO.
v4 changes one occupancy bit on update and searches at most four bitmap words
when BBO is read. Consequently v4's BBO query is not free, but frequent
best-level deletion no longer injects a 256-level scan into every update.

## ID-index deletion policy

The isolated fixed-live-set churn test performs 32,768 remove/fresh-insert
pairs after initial population. Backshift prevents deletion history from
accumulating into future probe paths.

| Policy | C++ median process p50 | Rust median process p50 |
| --- | ---: | ---: |
| tombstone | 6.206 ms | 6.020 ms |
| backward shift | 4.302 ms | 4.197 ms |

This supports a v5 full-book experiment, not an automatic replacement: each
backshift deletion can move a collision cluster. The index must keep compact
values, and the full-book v5 must preserve v4 as its control.

## Production-boundary components

- **Resource bound:** bitmap v4 reported zero allocations after construction
  across 24,576 updates in C++ and Rust.
- **Feed integrity:** gap detection refuses discontinuous incrementals; bounded
  snapshot replay emits only a contiguous suffix. Steady contiguous control
  processing measured 0.147 ms C++ and 0.587 ms Rust median process p50 for
  262,144 events. This is an in-process throughput observation, not event
  latency.
- **Read publication:** the BBO seqlock cell passed a concurrent 100,000-update
  invariant stress test. Its uncontended publish/read baseline was about 19 ns
  in C++ and 21 ns in Rust per pair; it needs native multi-core retry-rate work.

## What the data does not support

It does not show that C++ or Rust is generally faster, that the chosen hash is
universally best, that a WSL2 p99 predicts production tails, or that v5 will
improve the full book before it is implemented and measured. Initial default
hasher results compare ecosystem defaults; shared-hasher/parity results narrow
the algorithmic question but still include toolchain and allocator differences.

## Reproduction and next decisive experiment

Run the documented matrix with an idle machine and preserve raw artifacts.
The next decisive experiment is the separately named
[`bitmap_backshift_order_book_v5`](../../specs/bitmap_backshift_order_book_v5.md):
implement it beside v4, pass the same LLBT state/digest and allocation audit,
then rerun both matrix boundaries. Native Linux, CPU isolation/frequency policy
and hardware counters are prerequisites for stronger tail conclusions.

Detailed evidence is linked from the component reports:
[`mixed matrix`](2026-08-21-mixed-lifecycle-matrix-v1.md),
[`index churn`](2026-08-21-order-id-index-churn-v5.md),
[`feed recovery`](2026-08-21-feed-sequence-recovery-v1.md),
[`resource audit`](2026-08-21-bitmap-resource-audit-v4.md), and
[`BBO handoff`](2026-08-21-bbo-snapshot-uncontended-v1.md).
