# Compact tombstone L3-book candidate

The former bitmap-ladder control now stores a live order as a 16-byte packed
`{side + bounded price offset, quantity}` record while retaining its tombstone
order-ID index. It performs no backward-shift deletion. This is the current
production candidate for the bounded 256-level book; the backshift version is
kept only as an experimental control.

Validation: the shared L3 fixture, zero-update-allocation audit, C++ tests, and
Rust tests pass. The order-ID index and price ladders remain preallocated.

Pinned CPU-0 comparison: 15 rotated rounds, C++/Rust × compact-tombstone/
backshift, 200 samples per process (3,000 samples per variant). Raw data:
`results/local/packed-tombstone-vs-backshift-20260823T152602Z`.

| Variant | Language | pooled p50 | pooled p99 | process-median p50 | process-median p99 |
| --- | --- | ---: | ---: | ---: | ---: |
| compact tombstone | C++ | 1.082 ms | 2.796 ms | 1.045 ms | 2.445 ms |
| backshift | C++ | 1.675 ms | 3.502 ms | 1.687 ms | 3.108 ms |
| compact tombstone | Rust | 1.185 ms | 2.716 ms | 1.180 ms | 2.428 ms |
| backshift | Rust | 1.198 ms | 2.778 ms | 1.179 ms | 2.670 ms |

The compact tombstone candidate is 38% faster at C++ process-median p50, and
has a better p99 in both implementations. Rust central latency is effectively
equal; its compact tombstone p99 is 9% lower. This result is scoped to the
short-lived synthetic lifecycle and WSL2 desktop environment. It establishes a
better current candidate, not a claim that tombstones dominate at every live
occupancy or market regime.
