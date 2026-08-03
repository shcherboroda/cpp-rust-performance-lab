# cpp-rust-performance-lab

A reproducible research project for studying equivalent C++ and Rust implementations, beginning with small CPU and memory primitives and progressing toward selected low-latency trading-system components.

## Goals

- Build disciplined performance-engineering practice through controlled experiments.
- Produce results and methodology suitable for GitHub, a résumé, and technical publications.
- Develop foundations that could inform a reusable performance-analysis service.
- Document what is measured, how it is measured, and the limits of each conclusion.

## Comparison principles

- Treat C++ and Rust as engineering tools, not competitors in a language contest.
- Compare equivalent workloads, inputs, semantics, and correctness checks.
- State compiler, operating-system, machine, build, and runtime configuration with every result.
- Investigate generated code and measurement validity before drawing conclusions.
- Avoid claims that either language is inherently faster.

## Planned progression

1. CPU and memory primitives: sums, branches, dependency chains, and pointer chasing.
2. Data-structure and concurrency primitives: queues, event dispatch, and memory pools.
3. Trading-oriented components: market-data processing, order books, risk checks, and simplified execution pipelines.

## Environment and status

The current development environment is **Ubuntu 26.04 under WSL2**.

Initial results will be machine-, compiler-, operating-system-, and configuration-specific; they are not general language-performance claims.

Current status: **repository bootstrap and benchmark methodology design**.

## Layout

- `specs/` — workload definitions and measurement methodology.
- `cpp/` — future C++ implementations.
- `rust/` — future Rust implementations.
- `scripts/` — future reproducibility and analysis helpers.
