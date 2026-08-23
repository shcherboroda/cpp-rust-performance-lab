# Bitmap best-level v4

## Problem in v3

Dense ladder v3 stores one quantity per price. When an update empties the current best bid or ask, v3 scans up to all 256 ladder entries to find the next occupied price. That rare path can create a tail-latency cost proportional to ladder width.

## Single proposed change

Add one 256-bit occupancy bitmap per side, represented as four `u64` words. A set bit means the corresponding dense-ladder quantity is non-zero.

- On a zero-to-nonzero level transition, set its bit.
- On a nonzero-to-zero transition, clear its bit.
- To recover best bid, inspect bitmap words from high to low and use the most-significant set bit of the first nonzero word.
- To recover best ask, inspect words from low to high and use the least-significant set bit of the first nonzero word.

This changes only best-level recovery. Order-ID indexing, ladder range, event layout, trace, timing boundary, and protocol remain as in v3.

## Expected trade-off

The common update path gains one bitmap update when a level crosses zero. The old worst-case best-removal scan becomes bounded by four word checks and one bit operation. The benefit should be evaluated with a workload that actually removes the best level frequently; lifecycle churn alone may under-exercise the relevant path.

## Educational mapping

| Concept | C++ | Rust |
| --- | --- | --- |
| Bitmap storage | `std::array<std::uint64_t, 4>` | `[u64; 4]` |
| Highest occupied bit | `std::countl_zero` | `leading_zeros` |
| Lowest occupied bit | `std::countr_zero` | `trailing_zeros` |
| Safe zero test | test word before bit operation | test word before bit operation |

The code must not call a count-leading/trailing-zeros operation on a zero word; the specification tests the word first.
