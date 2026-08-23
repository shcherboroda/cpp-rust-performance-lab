# Parity order-book v2 — WSL2 baseline

## Status

Preliminary but repeatable WSL2 desktop observation. This is not a native-Linux or HFT latency claim.

## Implementation

The tested implementation is defined in [`specs/parity_order_book_v2.md`](../../specs/parity_order_book_v2.md). C++ and Rust share the same event sequence and the following structural properties:

- 40-byte event representation;
- fixed 65,536-slot order-ID index for 32,768 live orders;
- same SplitMix64 ID mixer;
- linear probing and tombstones;
- sorted contiguous bid/ask price-level arrays.

Both pass the same LLBT fixture and final state digest.

## Protocol

Each language ran 15 independent processes. Each process had 10 warm-ups and 200 retained samples, applying 65,536 pre-generated events per sample. C++/Rust process order alternated. Raw CSVs and environment metadata are local, ignored artifacts.

## Results

| Run mode | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | ---: | ---: | ---: |
| CPU 0 pinned | C++ | 2.849 ms | 5.155 ms | 2.824 ms |
| CPU 0 pinned | Rust | 1.876 ms | 3.539 ms | 1.867 ms |
| Unpinned | C++ | 2.912 ms | 4.957 ms | 2.915 ms |
| Unpinned | Rust | 2.045 ms | 3.849 ms | 1.999 ms |

For this workload and machine, Rust has lower p50 and p99 in both modes after container and layout parity is imposed. The result still includes distinct standard allocators, GCC versus rustc/LLVM code generation, and the languages' error-handling implementations.

## Interpretation

The earlier native-container observation and this v2 result answer different questions. The earlier result compared default ecosystem choices; v2 isolates a common data-structure algorithm more closely. Neither establishes a language-wide ranking.

Next investigation: inspect generated assembly and then change one property symmetrically—for example, replace the sorted vector level representation with a bounded dense price ladder—before considering language-specific reference implementations.

That dense-ladder experiment is v3. Its next isolated optimization, bitmap-assisted best-level recovery, is specified in [`specs/bitmap_best_level_v4.md`](../../specs/bitmap_best_level_v4.md).
