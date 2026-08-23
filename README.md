# cpp-rust-performance-lab

A reproducible research laboratory for evaluating C++ and Rust design choices in low-latency and HFT-oriented systems. The project investigates components, their implementations, and the trustworthiness of the measurements used to evaluate them.

It is not a language contest. A result is always scoped to a defined workload, implementation, machine, operating system, compiler, and runtime configuration.

## Objectives

- Build disciplined performance-engineering practice for low-latency systems.
- Compare equivalent C++ and Rust implementations, then compare the strongest practical implementation in each ecosystem.
- Explain material design differences: data layout, allocation, concurrency, safety boundaries, APIs, generated code, and operational trade-offs.
- Measure throughput and latency distributions, especially tail latency, under controlled and realistic workloads.
- Build a low-intrusion metrics and measurement layer whose own cost and bias are measured and reported.
- Produce reproducible evidence suitable for a public engineering portfolio, technical writing, and future reusable performance-analysis tooling.

## Research tracks

### 1. Semantic-parity implementations

For each selected component, implement the same algorithm and contract in C++ and Rust. Inputs, output, overflow/error semantics, data layout where applicable, correctness checks, and measurement boundary are held equivalent.

This isolates the effect of language and toolchain choices as far as a practical experiment can. It does **not** establish that one language is universally faster.

### 2. Reference implementations

For the same component, create the best practical implementation independently in C++ and Rust. Implementations may differ when that is justified by the language, ecosystem, or operational requirements.

Every meaningful difference must be explicit and documented:

- the requirement it addresses;
- the chosen approach in each language;
- the expected performance and safety consequences;
- the generated-code or runtime evidence supporting the explanation;
- the remaining non-equivalences that prevent a direct language-only conclusion.

This track answers the engineering question: which implementation and ecosystem best fit this particular low-latency component and operating context?

### 3. Measurement-system validation

Develop a metric-collection layer with a documented *perturbation budget*: the cost and bias it introduces into the system it observes.

Perfectly non-invasive in-process measurement is not possible: reading a timestamp, taking a branch, writing an event, or modifying cache and register pressure can change the workload. Instead, the project measures and reports the difference between:

1. no instrumentation;
2. installed but disabled hooks;
3. enabled hooks with a minimal handoff; and
4. full event capture and off-core aggregation.

The measurement layer must avoid allocation, locks, blocking I/O, formatting, and syscalls on the measured critical path. It should use preallocated per-core buffers and move aggregation/export off the critical core. When available, external or hardware-assisted observations are used to complement in-process data. Results include the measurement layer's own effects on cycles, instructions, cache behavior, throughput, and p50/p99/p99.9 latency.

## Development sequence

1. Select one bounded HFT-relevant component or mechanism and specify its contract, workload, and correctness oracle.
2. Build matched C++ and Rust implementations and establish a reproducible baseline.
3. Build and document independent reference implementations; explain their deliberate differences before comparing results.
4. Run controlled latency and throughput experiments, inspect generated assembly, and investigate unexpected results.
5. Add the low-intrusion measurement layer and quantify its perturbation budget against the uninstrumented baseline.
6. Extend the work from individual primitives to pipelines: feed decode, event dispatch, order-book updates, risk checks, strategy execution, and order encoding.

## Measurement principles

- Measure distributions, not a single timing number; report tail latency as well as throughput and central tendency.
- Fix and record the machine, CPU settings, OS/kernel, affinity/isolation policy, compiler versions and flags, build profile, input, and run protocol.
- Keep allocation, data generation, validation, reporting, and aggregation outside the timed region unless they are part of the workload being studied.
- Repeat runs, preserve raw samples, and treat noise and systematic measurement bias as separate problems.
- Validate correctness independently and prevent dead-code elimination.
- Inspect generated assembly and relevant hardware counters before attributing a difference to a language feature.
- Distinguish measured facts from explanations and from conclusions that may generalize.

## Current starting workload

`sequential_sum` is a methodology sanity check, not an HFT component. It provides matched C++ and Rust traversals over identical deterministic input with a shared checksum, warm-ups, repeated wall-clock samples, and summary statistics. Its specification is in [`specs/benchmark_sanity.md`](specs/benchmark_sanity.md).

## Environment and status

Current development environment: **Ubuntu 26.04 under WSL2**. Results from it are configuration-specific and unsuitable as universal low-latency claims. Later HFT-oriented experiments should run on a documented bare-metal or equivalently controlled Linux host.

Current status: **repository bootstrap, first measurement sanity workload, and an L3 order-book state-engine specification**. The first HFT-relevant experiment is defined in [`specs/l3_order_book.md`](specs/l3_order_book.md); its data-source decision is recorded in [`docs/adr/0001-l3-data-sources.md`](docs/adr/0001-l3-data-sources.md).

The first parity measurement protocol is [`specs/measurement_protocol.md`](specs/measurement_protocol.md). It produces WSL2 desktop baselines only; it does not make bare-metal low-latency claims.

The current order-book baseline's equalities and deliberate non-equivalences are tracked in [`specs/parity_audit.md`](specs/parity_audit.md).

The first fixed-layout parity result is recorded in [`docs/experiments/2026-08-21-parity-order-book-v2.md`](docs/experiments/2026-08-21-parity-order-book-v2.md).

The next symmetric cache-locality experiment is defined in [`specs/dense_price_ladder_v3.md`](specs/dense_price_ladder_v3.md).

The next isolated deletion/BBO-search change, and its preliminary WSL2 result, are documented in [`specs/bitmap_best_level_v4.md`](specs/bitmap_best_level_v4.md) and [`docs/experiments/2026-08-21-best-level-bitmap-v4.md`](docs/experiments/2026-08-21-best-level-bitmap-v4.md).

The cross-version order-book benchmark matrix, including stream, operation, search, resource, and measurement-perturbation workloads, is [`specs/order_book_benchmark_matrix.md`](specs/order_book_benchmark_matrix.md). Meaningful milestones receive an evidence-first publication assessment in [`docs/publication-notes/`](docs/publication-notes/).

Key production-oriented component changes are accompanied by a short design note explaining the problem, algorithm, trade-off, and validation boundary. The current order-ID index note is [`docs/components/fixed_order_index.md`](docs/components/fixed_order_index.md).

Formatting is part of the repository contract: C++ follows [`.clang-format`](.clang-format), Rust follows [`rust/rustfmt.toml`](rust/rustfmt.toml), and [`scripts/check_format.sh`](scripts/check_format.sh) verifies both before review.

## Layout

- `specs/` — workload contracts, methodology, and experiment records.
- `docs/` — architecture, decisions, and data-source notes.
- `cpp/` — C++ order-book library, tests, and benchmarks; current sanity benchmark is in `cpp/benchmarks/`.
- `rust/` — Rust order-book library, tests, and benchmarks; current sanity benchmark is in `rust/src/bin/`.
- `scripts/` — reproducibility, collection, and analysis helpers.
