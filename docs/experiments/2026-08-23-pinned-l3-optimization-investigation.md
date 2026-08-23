# Pinned bounded-L3 optimization investigation

Date: 2026-08-23. These are WSL2 desktop results, not native-Linux or
production-HFT certification. Release builds were pinned to CPU 0. Each process
has 10 warm-ups and 200 timed samples; one sample processes 57,344 synthetic
L3 events and verifies that the book becomes empty. `pooled` percentiles use
3,000 samples (15 processes × 200); `process-median` is the median of the 15
per-process percentiles. Raw local directories are named below.

## 1. Affinity baseline: v4 tombstones vs v5 backshift

The direct alternating series is documented in
`2026-08-23-bitmap-backshift-v5.md`.

| Variant | Language | pooled p50 | pooled p99 | process-median p50 |
| --- | --- | ---: | ---: | ---: |
| v4 tombstones | C++ | 1.352 ms | 2.621 ms | 1.360 ms |
| v5 backshift | C++ | 1.569 ms | 2.937 ms | 1.586 ms |
| v4 tombstones | Rust | 1.080 ms | 2.304 ms | 1.076 ms |
| v5 backshift | Rust | 1.180 ms | 2.553 ms | 1.211 ms |

v5 is slower by 17% in C++ and 13% in Rust at process-median p50 on this
low-live-set lifecycle. Backshift avoids tombstone accumulation, but its cluster
moves are not repaid when every cycle promptly removes its orders. It remains a
selectable policy for a long-lived, high-occupancy book, not a default.

## 2. Probe diagnostics and hash policy

The fixed index exposes a read-only probe summary. A controlled fill of 16,384
orders in 65,536 slots recorded 19,154 total probes: 1.169 per live entry and a
maximum of 8. Thus, a long collision cluster is not the likely explanation for
v5's regression on the mixed trace.

Pinned alternating hash A/B: `results/local/index-hash-interleaved-20260823T143848Z`.

| Hash | Language | pooled p50 | pooled p99 | process-median p50 |
| --- | --- | ---: | ---: | ---: |
| SplitMix finalizer | C++ | 3.304 ms | 5.326 ms | 3.241 ms |
| Multiplicative mix | C++ | 3.163 ms | 5.917 ms | 3.107 ms |
| SplitMix finalizer | Rust | 3.372 ms | 5.813 ms | 3.274 ms |
| Multiplicative mix | Rust | 2.677 ms | 5.633 ms | 2.580 ms |

Multiplication improves central latency by about 4% in C++ and 21% in Rust, but
worsens the C++ p99. It is a candidate, not a promoted default. Identity
hashing was stopped and rejected: sequential IDs plus a power-of-two table
created pathological clustering.

## 3. BBO cache-line alignment

`BboSnapshotCell` is 64-byte aligned in both languages; tests check alignment
and seqlock consistency. A deliberately unaligned twin exists only for a
controlled benchmark. Fifteen pinned processes alternated unaligned/aligned
order (`results/local/bbo-alignment-interleaved-20260823T144800Z`); each process
has 200 samples of 100,000 uncontended publish/read pairs.

| Language | Cell | process-median p50 | process-median p99 |
| --- | --- | ---: | ---: |
| C++ | unaligned | 1.197 ms | 2.006 ms |
| C++ | 64-byte aligned | 1.155 ms | 1.999 ms |
| Rust | unaligned | 1.822 ms | 2.852 ms |
| Rust | 64-byte aligned | 1.719 ms | 3.130 ms |

The uncontended central result is neutral-to-slightly-better; Rust's p99 is
worse. Alignment remains for its design purpose—isolating the writer's cache
line—not as a claimed latency win. A multi-thread false-sharing/read-contention
experiment is required before making that claim.

## 4. v6 compact order record

v6 retains v5's bounded backward-shift index and the 256-level bitmap ladder.
Only a live-order record changes: side and the bounded price offset share one
`u64`; quantity remains `u64`. The record is statically asserted at 16 bytes in
both languages instead of v5's 24-byte `{side, absolute_price, quantity}`. No
quantity narrowing occurs and the price range is validated before insertion.

The L3 fixture passes for C++ v5/v6 and Rust v6. v6 also passes zero-update-
allocation audits in both languages. C++ `OrderBook` remains 4,216 bytes because
the fixed ladders dominate it, but the 65,536-record index payload falls from
1.5 MiB to 1.0 MiB before slot metadata.

The direct rotated series is
`results/local/v5-v6-packed-interleaved-20260823T144850Z`: 15 rounds per mode,
each running C++/Rust × v5/v6 in a different order.

| Boundary | Variant | Language | pooled p50 | pooled p99 | process-median p50 | process-median p99 |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| update-only | v5 | C++ | 1.613 ms | 3.089 ms | 1.606 ms | 2.998 ms |
| update-only | v6 packed | C++ | 1.003 ms | 2.452 ms | 0.993 ms | 2.290 ms |
| update-only | v5 | Rust | 1.194 ms | 2.789 ms | 1.178 ms | 2.575 ms |
| update-only | v6 packed | Rust | 1.124 ms | 2.629 ms | 1.114 ms | 2.506 ms |
| update + BBO | v5 | C++ | 2.185 ms | 4.688 ms | 2.115 ms | 4.315 ms |
| update + BBO | v6 packed | C++ | 1.451 ms | 3.304 ms | 1.435 ms | 3.104 ms |
| update + BBO | v5 | Rust | 1.685 ms | 4.589 ms | 1.684 ms | 3.415 ms |
| update + BBO | v6 packed | Rust | 1.663 ms | 3.750 ms | 1.684 ms | 3.320 ms |

v6 improves C++ central update-only latency by 38% and update+BBO by 32%, with
better C++ tails. Rust improves 5.4% without BBO and is neutral with BBO; it
does not show a reason to reject the layout, but does not justify a strong Rust
speed claim either.

## Decision and next experiments

v6 is the best current integrated candidate: behavior is validated, updates do
not allocate, and it improves C++ without hiding a tail regression. This is a
measured decision, not a claim of a universally optimal book. Next independent
experiments are: high-live-set/long-history traces for tombstone vs backshift;
BBO writer-reader contention for false sharing; and hardware-counter collection
for cycles, instructions, cache misses and migrations with measurement-overhead
calibration. WSL2 desktop results remain comparative, not exchange-colocation
latency numbers.
