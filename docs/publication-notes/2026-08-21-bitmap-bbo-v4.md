# Publication assessment — bitmap BBO recovery v4

## Question

Can a four-word occupancy bitmap reduce recovery cost when the best price disappears from a bounded 256-price L3 ladder?

## Evidence collected

The v3 scan and v4 bitmap implementations share the event layout, fixed order index, hash mixer, price range, fixture, and digest. The focused workload removes the sole order at the best price and reads BBO after every event. Each version ran 15 alternating C++/Rust processes with 200 retained samples per process under WSL2.

## Observation

For this workload, median process p50 improved from 6.605 ms to 5.343 ms in C++ and from 5.970 ms to 2.086 ms in Rust. C++ v4 also exhibited two timing modes and a worse p99, so its tail result is not stable.

## Limits

This is not comparable to the update-only lifecycle workload: it includes two BBO reads after every event. It is a synthetic worst-case best-level-removal profile, not a venue event mix. WSL2 desktop scheduling prevents a trustworthy tail-latency or causal language conclusion.

## Publication decision

**Internal engineering note; not LinkedIn-ready as a performance claim.** It can later support a post about measurement discipline — specifically, why an optimization must be compared under an identical timed boundary — after confirmation on controlled native Linux and completion of the common benchmark matrix.
