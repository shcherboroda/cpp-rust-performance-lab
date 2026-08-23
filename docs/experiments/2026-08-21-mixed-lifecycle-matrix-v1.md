# Mixed L3 lifecycle matrix v1 — WSL2 baseline

## Status

This is a reproducible, preliminary WSL2 desktop baseline. It is useful for
algorithm selection and regression detection, but it is not an HFT deployment
claim and must be confirmed on controlled native Linux before publishing a
tail-latency conclusion.

## Question and controls

All four retained implementations process exactly the same pre-generated,
valid 57,344-event L3 trace described in
[`stream_mixed_lifecycle_v1.md`](../../specs/stream_mixed_lifecycle_v1.md).
Each cycle covers add, partial cancel, upsert, replace, full execute, and a
delete/order-delete. The digest after the timed loop verifies the same final
state. Construction, trace generation, validation, output, and aggregation
are outside the timing boundary.

Two distinct boundaries are deliberately reported:

| Boundary | Meaning |
| --- | --- |
| `update-only` | Apply all events; BBO need not be observable synchronously after each event. |
| `update+BBO` | Apply every event and read bid and ask immediately afterwards. |

For each language/version pair: 15 independent alternating processes, 10
warm-ups and 200 retained samples per process. Thus each pooled row has 3,000
samples. The selected raw artifacts are retained locally under `results/local/`:
`native_mixed_lifecycle-20260821T162748Z`,
`parity_mixed_lifecycle-20260821T163155Z`,
`dense_mixed_lifecycle-20260821T163209Z`,
`bitmap_mixed_lifecycle-20260821T163250Z`, and the corresponding BBO directories
with timestamps `163434Z`, `163301Z`, `163315Z`, and `163358Z`.

## Results

### Update-only

| Version | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| v1 native containers | C++ | 2.109 ms | 6.717 ms | 1.992 ms |
| v1 native containers | Rust | 1.916 ms | 6.288 ms | 1.892 ms |
| v2 parity index + sorted levels | C++ | 1.380 ms | 7.419 ms | 1.219 ms |
| v2 parity index + sorted levels | Rust | 1.232 ms | 4.320 ms | 1.206 ms |
| v3 dense ladder + cached BBO | C++ | 6.088 ms | 10.363 ms | 5.814 ms |
| v3 dense ladder + cached BBO | Rust | 5.529 ms | 11.536 ms | 5.518 ms |
| v4 bitmap dense ladder | C++ | 1.094 ms | 3.252 ms | 1.084 ms |
| v4 bitmap dense ladder | Rust | 1.156 ms | 3.714 ms | 1.130 ms |

### Update+BBO

| Version | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| v1 native containers | C++ | 3.914 ms | 11.809 ms | 3.889 ms |
| v1 native containers | Rust | 3.645 ms | 11.979 ms | 3.594 ms |
| v2 parity index + sorted levels | C++ | 1.479 ms | 4.248 ms | 1.451 ms |
| v2 parity index + sorted levels | Rust | 1.241 ms | 4.194 ms | 1.189 ms |
| v3 dense ladder + cached BBO | C++ | 6.292 ms | 10.520 ms | 6.334 ms |
| v3 dense ladder + cached BBO | Rust | 6.114 ms | 12.177 ms | 6.238 ms |
| v4 bitmap dense ladder | C++ | 1.779 ms | 4.665 ms | 1.753 ms |
| v4 bitmap dense ladder | Rust | 1.447 ms | 5.338 ms | 1.391 ms |

## Explanation of the v3/v4 difference

The apparent v4 update-only advantage is not a measurement error and is not a
free BBO operation. It comes from *where the work is paid*.

v3 stores cached best prices. When an event empties the current best level,
`subtract_level` immediately calls `refresh_best`, which scans as many as 256
quantity entries. That scan is inside the update path even when no caller asks
for BBO. See [`dense_ladder_order_book.cpp`](../../cpp/src/dense_ladder_order_book.cpp).

v4 maintains one occupancy bit per price level. A zero transition sets or
clears one bit during an update; `best_bid`/`best_ask` scan at most four 64-bit
bitmap words only when queried. See
[`bitmap_ladder_order_book.cpp`](../../cpp/src/bitmap_ladder_order_book.cpp).

The trace intentionally drains levels frequently, so this placement difference
is exposed. Adding BBO reads raises the v4 C++ median-process p50 from 1.084
to 1.753 ms and Rust from 1.130 to 1.391 ms, while v3 changes little because it
already paid its recovery scan on update. The engineering choice therefore
depends on the contract: update-only consumers may prefer deferred BBO work;
an immediate-consistency consumer should use the `update+BBO` boundary.

## Limits and next decision

The benchmark does not yet isolate each operation, vary live occupancy, audit
allocations, or measure hardware counters. WSL2 scheduling is visible in the
tails. The matrix establishes a common baseline, not a language ranking. The
next controlled change is to compare tombstone and backward-shift order-ID
indexes under long-lived, high-churn histories before deciding whether to
integrate either policy into the production-oriented book.
