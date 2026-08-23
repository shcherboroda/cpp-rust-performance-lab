# Publication assessment — backshift order index v5

## Question

Can the order-ID index avoid unbounded probe-chain degradation caused by tombstones during a long-lived order-book session?

## Work completed

A reusable fixed-capacity, linear-probing `FixedOrderIndex<Value>` was implemented in C++ and Rust. It uses backward-shift deletion, so an erase restores an empty terminating slot instead of retaining a tombstone. Both implementations pass collision-cluster, long-history, and 50,000-step deterministic model-based tests against a reference map.

## What is not yet measured

The component has not yet been benchmarked against the tombstone index under controlled live load, collision shape, and churn history. It is not yet integrated into an order-book version, so no end-to-end book result exists.

## Publication decision

**Internal engineering note; not LinkedIn-ready.** The useful public story begins only after the component has both a controlled index benchmark and a symmetric order-book integration, ideally confirmed on native Linux.
