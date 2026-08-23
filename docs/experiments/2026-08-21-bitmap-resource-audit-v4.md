# Bitmap order-book resource audit v4

## Question

Does the production-oriented bitmap book allocate on its update path after its
explicit construction-time capacity has been established?

## Audit boundary

Both language-specific audit executables construct a bitmap book for 32,768
live orders, reset an allocation-call counter, then apply 8,192 valid
add/partial-cancel/full-execute cycles (24,576 updates). The count covers only
allocations initiated after construction and before the assertion; formatting,
test startup and construction are outside the interval.

| Implementation | Update-path allocation calls | Result |
| --- | ---: | --- |
| C++ bitmap v4 | 0 | Pass |
| Rust bitmap v4 | 0 | Pass |

The C++ audit overloads the process allocation operator for the test binary.
The Rust audit installs a counting wrapper over `System`. This proves the
defined workload's no-allocation invariant; it does not prove that all future
changes, error handling paths, logging, adapters, or caller-owned containers
are allocation-free.

## Resource bounds

The book allocates a fixed power-of-two ID table sized for at most 50% live
load at construction, plus two fixed 256-entry quantity ladders and two
four-word occupancy bitmaps. Its hot update operations contain no dynamic
growth. The L3 event trace, caller output buffers, and feed-recovery replay
buffer must likewise be reserved by their owning layer before a latency
critical interval.

The auditable executables are
[`bitmap_allocation_audit.cpp`](../../cpp/tests/bitmap_allocation_audit.cpp)
and
[`bitmap_allocation_audit.rs`](../../rust/src/bin/bitmap_allocation_audit.rs).
