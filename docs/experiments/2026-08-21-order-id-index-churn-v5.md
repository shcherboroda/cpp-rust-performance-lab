# Order-ID index deletion policy v5 — WSL2 baseline

## Question

Which deletion policy better preserves order-ID lookup/insertion performance
under a long-lived feed that constantly removes and adds orders while keeping
the live set fixed?

This is an isolated component benchmark, deliberately separate from price
levels and BBO. It does **not** yet prove the faster policy improves the full
order book.

## Controlled component and workload

Both implementations use the same power-of-two fixed table, 50% maximum live
load, SplitMix64 ID mixer, linear probing, `u64` key and `u32` value. There is
no allocation after construction. They begin with 16,384 live entries, then
perform 32,768 deterministic operations; each operation removes one live ID
and inserts a fresh ID, keeping the live count fixed.

`tombstone` marks a deleted slot and reuses it later. `backshift` shifts only
the subsequent entries whose probe path crosses the new hole, restoring an
empty terminator and preventing historical tombstone accumulation. The exact
backshift component and its randomized model tests are in
[`fixed_order_index.hpp`](../../cpp/include/llab/fixed_order_index.hpp) and
[`fixed_order_index.rs`](../../rust/src/fixed_order_index.rs).

## Protocol and results

15 alternating independent C++/Rust processes; 10 warm-ups and 200 retained
samples each (3,000 pooled samples per language/policy). The timed region is
the 32,768 remove/insert pairs; construction and initial population are not
timed.

| Policy | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| tombstone | C++ | 6.184 ms | 10.365 ms | 6.206 ms |
| backshift | C++ | 4.191 ms | 7.607 ms | 4.302 ms |
| tombstone | Rust | 6.123 ms | 10.662 ms | 6.020 ms |
| backshift | Rust | 4.511 ms | 8.327 ms | 4.197 ms |

For this high-churn history, median-process p50 improves by about 31% in C++
and 30% in Rust. Raw artifacts are local under
`results/local/tombstone_order_index_churn-20260821T164045Z` and
`results/local/backshift_order_index_churn-20260821T164130Z`.

## What this means — and does not mean

The observation supports the mechanism: tombstones make future probing depend
on *history*, while backshift bounds the live table by current occupancy. It
does not make backshift unconditionally superior. A deletion can move several
compact values, so collision-heavy clusters or very large values can reverse
the trade-off. The production design must therefore index a compact,
trivially-movable order handle, not a large order payload, and must rerun the
common L3 matrix after integration.

The next change will integrate the policy only behind a distinct versioned
book, preserve the existing tombstone version as a control, verify identical
LLBT state/digest, and measure both mixed lifecycle boundaries. Native-Linux
confirmation is required before publishing a latency claim.
