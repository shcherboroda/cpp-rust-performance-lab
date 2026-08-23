# Bitmap best-level recovery v4 — WSL2 baseline

> **Comparison boundary:** the timings in this document are comparable only
> between v3 scan and v4 bitmap under the `best_level_churn` workload.  They
> must not be compared to the v1/v2/v3 lifecycle timings: this workload reads
> both BBO values after every event, while the lifecycle workload reads them
> only after the timed event loop.

## Question

Does a 256-bit occupancy bitmap reduce the cost of exposing BBO after the best
price becomes empty, compared with v3's scan of a 256-entry dense ladder?

This is a targeted question about one update/query pattern.  It does not
measure a venue's full event distribution and does not support a general
ranking of C++ and Rust.

## Controlled difference

Both v3 and v4 retain the 40-byte event format, a 65,536-slot fixed
open-addressed order index, SplitMix64 mixing, linear probing/tombstones, and
a 256-price dense ladder.  Both pass the shared LLBT fixture and final state
digest.

v3 stores current best prices and, when that level becomes empty, scans the
quantity ladder to recover the next best.  v4 adds a four-word (`4 x u64`)
occupancy bitmap per side.  A level's bit changes only when aggregate quantity
crosses zero; BBO uses a high-to-low or low-to-high bit scan over at most four
words.

The workload is defined in
[`specs/best_level_churn_workload.md`](../../specs/best_level_churn_workload.md).
It applies 32,768 add/execute pairs per sample and reads BBO after every event.

## Protocol

CPU 0 affinity; 15 independent alternating C++/Rust processes per version;
10 warm-up and 200 retained samples per process.  Each row aggregates 3,000
raw samples.  The environment is WSL2 on an otherwise idle desktop, so this is
a preliminary baseline only.

## Results

| Version | Language | pooled p50 | pooled p90 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: | ---: |
| v3 scan | C++ | 6.695 ms | 9.108 ms | 11.856 ms | 6.605 ms |
| v4 bitmap | C++ | 5.441 ms | 10.809 ms | 14.269 ms | 5.343 ms |
| v3 scan | Rust | 5.911 ms | 10.655 ms | 12.026 ms | 5.970 ms |
| v4 bitmap | Rust | 2.121 ms | 10.379 ms | 11.953 ms | 2.086 ms |

In this setup, the median process p50 changes by about **−19%** for C++ and
**−65%** for Rust.  C++ v4 process medians split into two visible modes
(about 2.6–3.1 ms in six processes and 5.3–9.9 ms in nine); its p99 is worse
than v3.  That variation is evidence that the WSL2 desktop environment remains
too noisy to make a fine-grained tail-latency conclusion.

## Interpretation

The bitmap reduces the expected algorithmic search bound from up to 256
quantity entries to at most four nonzero bitmap-word checks plus one bit scan.
The results are consistent with a central-tendency benefit, especially for
Rust, but do not establish that C++ generates inferior code or that the bitmap
will improve a realistic market-data distribution.  The next step is to run
the same experiment on controlled native Linux and inspect generated assembly
and hardware counters before drawing a causal conclusion.

## Required follow-up comparison matrix

For a version-to-version performance narrative, each implementation must run
both of these fixed measurement boundaries:

| Boundary | What it answers |
| --- | --- |
| `update_only_lifecycle` | Cost of applying order events without a BBO read on every update. |
| `update_and_bbo_churn` | Cost of applying events when BBO must be observable after each update. |

Only values within one row can be compared directly.  The first existing v1
baseline belongs to `update_only_lifecycle`; the current bitmap result belongs
to `update_and_bbo_churn`.  Future work will run every retained version under
the relevant common boundary or explicitly mark it as not comparable.
