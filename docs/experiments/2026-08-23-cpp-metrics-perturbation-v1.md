# C++ metrics perturbation calibration v1

The compact tombstone bitmap book was replayed on pinned CPU 0 in three modes:
no hook, an installed compile-time-disabled `Scope<false>`, and per-event TSC
capture into a preallocated thread-local ring. The ring is constructed before
the timed interval. Each mode has 15 rotated processes and 200 samples per
process; a sample replays 57,344 events. Raw data is local at
`results/local/cpp-metrics-calibration-20260823T153042Z`.

| Mode | pooled p50 | pooled p99 | process-median p50 | process-median p99 |
| --- | ---: | ---: | ---: | ---: |
| no hook | 0.766 ms | 2.413 ms | 0.790 ms | 2.145 ms |
| compile-time disabled | 0.741 ms | 2.325 ms | 0.736 ms | 2.141 ms |
| full per-event capture | 4.145 ms | 10.583 ms | 4.130 ms | 7.449 ms |

The disabled path is indistinguishable from the baseline within this WSL2
desktop noise band; its small apparent improvement is not interpreted as a
speedup. Full capture adds about 5.2× to central latency and 3.5× to the
process-median p99. It is therefore a diagnostic mode only. The next required
mode is sampled capture, followed by the symmetric Rust calibration.

The result validates the design rule rather than a desired outcome: metrics
must report their perturbation and the production path must never silently run
full per-event timing.
