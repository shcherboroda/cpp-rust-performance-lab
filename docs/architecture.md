# Repository architecture

## Design rule

The repository separates reusable order-book code from venue-specific data ingestion and from the experiments used to evaluate them. No benchmark may make a feed adapter, parser, network stack, or reporting path appear to be part of the order-book engine unless that is explicitly the experiment being measured.

## Target layout

```text
docs/                         architecture, decisions, data-source notes
specs/                        contracts and experiment protocols
cpp/
  include/llab/               public C++ library interface
  src/                        C++ library implementation
  tests/                      C++ correctness and cross-language fixtures
  benchmarks/                 standalone C++ benchmark executables
rust/
  src/                        Rust library implementation
  src/bin/                    standalone Rust benchmark executables
  tests/                      Rust correctness and cross-language fixtures
adapters/
  bitfinex/                   live Raw Books capture/normalization (future)
  nasdaq_itch/                ITCH sample parser/normalization (future)
data/
  fixtures/                   small versioned test traces only
  captures/                   ignored local live recordings
scripts/                      capture, replay, build, and result analysis
results/                      ignored raw output; published reports are explicit artifacts
```

`adapters/` is deliberately language-neutral at the repository level. An adapter may have C++, Rust, or shared-format implementations, but its output contract is defined in `specs/`, rather than by one language's types.

## Layer boundaries

1. **Source adapter** acquires and parses venue data, validates source sequencing, and emits a normalized trace. It is not part of the initial book-engine timing boundary.
2. **Trace/fixture layer** stores deterministic canonical events and expected state digests. It enables identical input for C++ and Rust.
3. **Order-book library** applies events and exposes the state required by the experiment.
4. **Benchmark harness** prepares state, times only the declared operation, validates results, and writes raw samples.
5. **Measurement layer** is introduced later and reports its own perturbation budget separately.

## Extension policy

Adding a venue must add an adapter and a source note, not alter core semantics to fit a transport format. Adding a new core representation or optimization requires a specification update, an implementation note, correctness fixtures, and separate parity/reference benchmark results.

Local captured market data, build trees, and raw measurement output are not committed by default. Small deterministic fixtures may be committed only when their license permits redistribution and their provenance is recorded.
