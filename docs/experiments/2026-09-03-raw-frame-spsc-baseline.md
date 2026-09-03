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

Seven independent C++/Rust process pairs alternated. Each process retained 20
samples. The process was restricted to WSL-visible CPUs 0 and 2. The producer
and consumer were **not individually pinned**, so this is a feasibility
baseline, not a controlled cross-language comparison.

## Results

| Language | Median of process p50 | Median of process p99 |
| --- | ---: | ---: |
| C++ | 54.659 ns/message | 61.957 ns/message |
| Rust | 44.404 ns/message | 54.722 ns/message |

One C++ process p99 was 164.541 ns/message; Rust process p99 ranged from
49.994 to 82.003 ns/message. This spread is material on the scale being
measured.

## Decision

Keep the bounded queue: unit tests verify FIFO order plus explicit `Full` and
`PayloadTooLarge` refusal, and all storage is allocated at construction. Do
not retain a C++-versus-Rust performance claim from these numbers. The next
measurement revision must pin producer and consumer individually to known
separate physical cores, record the topology and retain raw samples.
