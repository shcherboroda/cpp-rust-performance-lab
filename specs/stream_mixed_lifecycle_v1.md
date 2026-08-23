# Deterministic mixed lifecycle stream v1

## Purpose

`stream_mixed_lifecycle_v1` is the first common event-processing boundary for
every retained order-book version. It measures a deterministic valid L3 update
stream with all supported mutation types. It is an operation-coverage stream,
not a claim that its proportions reproduce a particular venue.

## Timed boundary

For each sample, construct a preallocated empty book, start the timer, apply
the pre-generated events in order, stop the timer, then verify the final state
and digest outside the timed region. The `with_bbo` companion reads bid and ask
after every event and folds them into a digest; it is a separate boundary.

## Cycle

One cycle uses fresh order IDs and returns the book to its initial empty state:

| Step | Event | Order state after event | Reason |
| ---: | --- | --- | --- |
| 1 | `Add A` | A live, quantity 10 | New ID and level activation. |
| 2 | `Add B` | A + B live | Second side/level. |
| 3 | `Cancel A, 3` | A quantity 7 | Partial reduction. |
| 4 | `OrderUpsert B, 15` | B quantity 15 | Existing-ID state replacement. |
| 5 | `Replace A -> C, 7` | C live, A removed | New-ID replacement path. |
| 6 | `Execute C, 7` | C removed | Full execution and possible best-level removal. |
| 7 | `Delete` or `OrderDelete B` | Empty | Venue deletion paths alternate by cycle. |

Price and side vary deterministically over a bounded 256-tick range. Quantities
and IDs are deterministic and valid. The cycle's final empty state prevents
history from one cycle changing the semantic workload of the next; index
history is intentionally measured separately by the long-churn index suite.

## Required implementations

Every order-book version that supports the L3 contract runs both:

1. `stream_mixed_lifecycle_v1` — update only;
2. `stream_mixed_lifecycle_with_bbo_v1` — update plus two BBO reads per event.

The results answer different questions and must never be compared directly.

## Correctness oracle

Before timing, each benchmark verifies that its generated trace is accepted.
After timing, it verifies zero live orders, no BBO, and a deterministic digest
of the BBO-read accumulator where applicable. C++ and Rust additionally run a
shared fixture for the same event contract.
