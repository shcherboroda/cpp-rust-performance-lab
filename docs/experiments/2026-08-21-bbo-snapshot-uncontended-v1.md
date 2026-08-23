# BBO snapshot handoff v1 — uncontended baseline

The seqlock BBO cell was measured in one thread: each of 10,000,000 iterations
publishes a four-field snapshot and immediately reads it back. Ten alternating
manual C++/Rust process invocations produced a median elapsed time of about
194 ms for C++ and 210 ms for Rust: roughly 19 and 21 ns per publish/read pair.

This measures the uncontended lower bound only. It does not measure reader
retry rate, cache-line contention, cross-core placement, scheduler interference
or end-to-end book-to-reader latency. The concurrent correctness tests establish
that accepted snapshots retained their cross-field invariant for 100,000 writer
updates, but no language comparison or HFT tail claim follows from this
desktop/WSL2 microbenchmark.
