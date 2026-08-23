# Low-intrusion metrics layer v1

## Purpose

The metrics layer is a reusable component, not part of the order-book
algorithm. It observes a critical path while making its own perturbation
explicitly measurable. A metric value without the cost of obtaining it is not
accepted as a low-latency result.

## Critical-path contract

The recording path must not allocate, lock, format, write I/O, invoke a syscall
or wait for a consumer. It owns a fixed-capacity buffer constructed before the
measured workload. A full buffer increments a dropped-sample counter; it never
blocks or resizes. Aggregation/export happens after the timed run or on a
different thread/core.

`Disabled` instrumentation is compile-time removable. `Sampled` timing checks a
power-of-two sequence mask and timestamps only selected operations. `Capture`
records every requested interval and is diagnostic-only, not assumed suitable
for a production hot path.

## Clock and portability

The first implementation uses a serialized x86 TSC only when the platform
supports it; it records the auxiliary CPU identifier at the ending read. A
sample that migrated CPU is marked invalid by the consumer. It measures cycles,
not wall-clock nanoseconds. Wall-clock conversion is a separate calibrated
operation. Unsupported targets expose no cycle clock rather than silently
pretending that a low-overhead clock exists.

## Required perturbation experiment

For every integrated component, run the same trace in four modes:

1. no metric object or hook;
2. installed compile-time-disabled hook;
3. sampled recorder with a preallocated ring; and
4. full capture with the same ring.

Report p50/p99/p99.9, throughput, all dropped/invalid samples, and the delta
from mode 1. A result is rejected if the metric layer silently loses samples or
if a claim omits its perturbation mode.

## WSL2 limitation

The current WSL2 environment reports a TSC clocksource, but its `perf` interface
does not expose the requested `cycles`, `instructions`, or `cache-misses`
events. In-process cycle samples can be calibrated here; PMU evidence and
production tail claims wait for native Linux.
