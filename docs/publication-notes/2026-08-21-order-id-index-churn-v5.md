# Publication assessment — index deletion policy v5

## Decision

**Needs native-Linux confirmation.** The mechanism and the 30% central-tendency
improvement are a worthwhile research finding, but the data is still WSL2 and
the policy is not yet measured inside the complete order book.

## Potential post angle

“In a fixed open-addressed order-ID index, the deletion policy can matter more
than the hash function once a feed has a long cancellation history. We compared
tombstones with backward-shift deletion under a controlled fixed-live-set
churn workload. Backshift reduced median process p50 by roughly 30% in both
C++ and Rust—not because either language is intrinsically faster, but because
the table stops carrying historical deletion debt. The next question is the
only one that matters for production: does it survive integration into the
whole L3 book and native-Linux measurement?”

Link the experiment rather than presenting the values as a general language
ranking: [`order-id-index-churn-v5`](../experiments/2026-08-21-order-id-index-churn-v5.md).
