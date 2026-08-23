# Feed sequence recovery v1 — correctness component and baseline

## Delivered contract

[`FeedSequenceRecovery`](../../specs/feed_sequence_recovery_v1.md) is a
bounded single-writer state machine between a venue adapter and an L3 book. It
does not let a caller apply an incremental message after a sequence gap. It
requires a snapshot, buffers post-snapshot incrementals in fixed storage, then
emits only a contiguous replay suffix. Duplicate/stale messages are ignored;
replay-buffer overflow and replay gaps fail recovery explicitly.

The C++ and Rust versions pass tests for out-of-order replay, duplicates, a
live-stream gap, replay gap, and bounded-buffer overflow. They allocate their
replay buffer only at construction. The caller owns the output vector and must
reserve it outside its hot path.

## Steady-state measurement

The benchmark begins in a completed snapshot, then applies 262,144 contiguous
incrementals to the recovery component and an already-reserved output vector.
It measures control-plane validation and handoff only—not parsing, order-book
mutation, snapshot construction, recovery sorting, or network I/O.

15 alternating C++/Rust processes, each with 10 warm-ups and 200 retained
samples, yielded 3,000 samples per language.

| Language | pooled p50 | pooled p99 | median process p50 | p50/event |
| --- | ---: | ---: | ---: | ---: |
| C++ | 0.155 ms | 0.447 ms | 0.147 ms | 0.59 ns |
| Rust | 0.608 ms | 1.649 ms | 0.587 ms | 2.32 ns |

Raw artifacts: `results/local/feed_sequence_recovery-20260821T164740Z`.

## Interpretation and limits

This only demonstrates that the steady-state checks are small relative to the
measured loop under this toolchain and WSL2 desktop. Sub-nanosecond derived
per-event values are throughput averages, not single-event latency, and are
not meaningful tail-latency claims. The C++/Rust difference may reflect
inlining, bounds checks, assertion/codegen differences, or measurement noise;
it is not evidence of a general language advantage.

The recovery path itself sorts a bounded buffer and is intentionally not folded
into the hot incremental number. A later fault-recovery benchmark must measure
snapshot/replay separately with real venue protocol semantics.
