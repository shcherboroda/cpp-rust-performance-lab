# Measurement protocol v1

## Scope

This protocol evaluates the semantic-parity C++ and Rust order-book implementations. It does not yet assess feed adapters, parser latency, networking, allocation strategies selected by independent reference implementations, or the future metrics system.

## Lifecycle-churn workloads

Each sample applies 65,536 pre-generated canonical events to an empty, pre-capacitated order book:

- 32,768 deterministic `Add` events with alternating sides, 128 tick offsets, and quantities 1–97;
- 32,768 matching full `Execute` events in identifier order.

The trace begins and ends with an empty book. Event generation and capacity reservation occur outside the timed interval. The timer surrounds only `OrderBook::apply` / `OrderBook::apply`-equivalent calls for the pre-generated events. Post-timing correctness checks verify that the resulting book is empty.

`order_book_lifecycle_churn_v1` uses native standard-library containers. `parity_order_book_lifecycle_churn_v2` applies the same events to the fixed-table/vector implementation defined in [`parity_order_book_v2.md`](parity_order_book_v2.md). Neither is a calibrated representation of Bitfinex or Nasdaq event distributions; they must not be reported as realistic market profiles.

## Sampling protocol

- One process run performs 10 warm-up samples and retains 200 measured samples.
- One comparison performs 15 independent process runs per language, for 3,000 retained samples per language before any cross-run aggregation.
- Process order alternates by round: C++ then Rust on odd rounds, Rust then C++ on even rounds. This reduces systematic drift from thermal state and background load.
- Every process writes its raw sample durations to a CSV file. Summaries are convenience output only; conclusions use raw data and preserve process-run identity.
- Per-process reporting uses nearest-rank p50, p90, p95, p99, and p99.9. Cross-process aggregation must not silently merge samples without retaining run boundaries.

## Required metadata

Every comparison directory contains the shell environment, `uname`, CPU information when available, compiler versions, build commands, UTC timestamp, run order, raw CSVs, and each program's stdout summary. Record power source, Windows host load, foreground applications, and any affinity choice manually in the experiment note.

## WSL2 and desktop-host limits

WSL2 is useful for correctness and preliminary relative baselines, but it is not a controlled low-latency environment. The Windows scheduler, host processes, virtualization, power policy, thermal state, and background programs can perturb timing. Telegram, browsers, IDEs, updates, and battery-saving modes can affect tail samples even if the Linux process itself is pinned.

For a **baseline**, do not close applications solely to make a result look better; record the state instead. For a **controlled comparison**, before starting:

1. connect the laptop to AC power and select a performance-oriented power mode;
2. close or pause high-CPU/high-I/O applications and scheduled work, including browser tabs, IDE indexing, sync clients, and downloads;
3. avoid active video calls, screen recording, or large file transfers;
4. optionally pin the measured process to one WSL-visible CPU with `LLAB_CPU=<cpu>`; and
5. leave the machine otherwise idle throughout the full alternating run.

These steps reduce noise; they do not turn WSL2 into a bare-metal HFT test host. Any low-latency claim still requires later confirmation on a documented native Linux system with controlled CPU isolation and frequency policy.

## Interpretation rules

- Never select the fastest individual sample or process run as the result.
- Treat a change in central tendency without tail or run-to-run analysis as inconclusive.
- Investigate regressions or improvements with generated assembly and hardware counters before attributing them to the language.
- Report an experiment as a WSL2 desktop baseline unless it meets a later controlled-host protocol.
