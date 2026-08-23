# Bitmap backshift order book v5 — WSL2 comparison

v5 keeps v4's 256-price bitmap ladder and replaces only its tombstone order-ID
index with the bounded backward-shift `FixedOrderIndex<Order>`. The shared mixed
L3 trace completed with an empty book and matching C++ v5 verification digest.

15 alternating C++/Rust processes, 10 warmups and 200 samples per process:

| Boundary | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| update-only | C++ | 1.605 ms | 3.642 ms | 1.594 ms |
| update-only | Rust | 1.220 ms | 2.653 ms | 1.194 ms |
| update+BBO | C++ | 2.065 ms | 4.497 ms | 2.071 ms |
| update+BBO | Rust | 1.678 ms | 3.796 ms | 1.614 ms |

## Same-session v4/v5 interleaving

The common update-only workload was rerun in one 15-round interleaved series:
each round executed C++ and Rust for both versions, and v4/v5 order alternated.

| Version | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| v4 tombstones | C++ | 1.352 ms | 2.621 ms | 1.360 ms |
| v5 backshift | C++ | 1.569 ms | 2.937 ms | 1.586 ms |
| v4 tombstones | Rust | 1.080 ms | 2.304 ms | 1.076 ms |
| v5 backshift | Rust | 1.180 ms | 2.553 ms | 1.211 ms |

Under this particular trace, v5 is slower by approximately 17% in C++ and 13%
in Rust using median-process p50. That does not contradict the isolated index
churn result: the mixed lifecycle has only a tiny live set at any instant and
ends every cycle empty. Tombstone history therefore does not create enough
future probe cost to repay backward-shift moves. The integrated result rejects
backshift as the default policy for this workload; it remains a selectable
policy for a long-lived, high-occupancy book and requires that representative
workload before reconsideration.
