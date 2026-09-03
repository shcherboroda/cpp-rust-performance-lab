# Raw-frame SPSC baseline (not a language conclusion)

## Hypothesis

A bounded queue with preallocated slots and payload buffers can hand off the
Bitfinex raw-frame shape without steady-state allocations or silent loss.

## Contract tested

The benchmark transfers 1,000,000 frames per sample through a 1,024-slot SPSC
queue. The deterministic payload mix is 15 × 64-byte frames followed by one
2,048-byte frame; slot payload capacity is 4,096 bytes. The consumer checks
capture-index order and reuses its output buffer. Network, JSON parsing, disk
I/O, clocks per event and book application are excluded.

The initial two-CPU process-affinity run was superseded. The retained series
uses 15 independent alternating C++/Rust process pairs with 20 samples per
process. In every process, the producer is pinned to WSL-visible CPU 0 and the
consumer to CPU 2. `lscpu` reports these as separate physical cores. The host
is an i7-1255U under WSL2, so the result remains a desktop baseline rather than
an HFT deployment claim.

## Results

| Language | Median of process p50 | Median of process p99 |
| --- | ---: | ---: |
| C++ | 54.125 ns/message | 63.704 ns/message |
| Rust | 44.893 ns/message | 55.838 ns/message |

The 15 process p50 values were 51.513–56.327 ns/message in C++ and
42.499–55.205 ns/message in Rust. Process p99 ranged from 58.262 to 87.427 ns
in C++ and from 46.519 to 104.983 ns in Rust. The tails remain noisy at this
scale despite thread affinity.

## Decision

Keep the bounded queue: unit tests verify FIFO order plus explicit `Full` and
`PayloadTooLarge` refusal, and all storage is allocated at construction. This
matched baseline is evidence that the Rust implementation is faster on this
specific WSL2 laptop workload (about 17% at median process p50), not evidence
of a general language advantage. The next measurement revision should retain
machine-readable raw samples and repeat on native Linux before using a tail
claim in publication.
