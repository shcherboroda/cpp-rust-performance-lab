# Semantic-parity audit — order-book baseline v1

## What is held equal

- Canonical event semantics, order identifiers, side/price/quantity types, and fixture digest.
- The `order_book_lifecycle_churn_v1` event sequence, capacity hint, warm-up count, measured-sample count, and timing boundary.
- Release optimization level: C++ CMake `Release` (`-O3 -DNDEBUG`) and Rust release `opt-level = 3`, without LTO.
- Full `u64` order-ID hash mixer: the same SplitMix64 finalizer is used in both languages, with no random per-process seed.

## Deliberate remaining non-equivalences

- C++ uses the implementation's `std::unordered_map` and `std::map`; Rust uses `HashMap` and `BTreeMap`. Their bucket/tree layouts, growth policies, and allocators differ. Replacing them with a custom representation is a later, separately documented optimization track.
- C++ errors are represented by exceptions; Rust errors by `Result`. The benchmark input is validated and generated as valid before timing, but the APIs still retain their native error mechanism.
- GCC generates the C++ binary and rustc/LLVM generates the Rust binary. This is an intentional toolchain comparison dimension, not proof of an intrinsic language property.

## Required evidence before attribution

1. Record event and order-book object sizes, plus actual order-index capacity after the common capacity hint, from each benchmark output.
2. Compare generated assembly for the hot apply path under the recorded release settings.
3. Use hardware counters on a controlled native Linux host; WSL2 does not expose the required PMU events in this environment.
4. Repeat the alternating process protocol with a documented CPU-affinity choice and inspect inter-process distributions.

## Interpretation

This audit establishes semantic parity, not bit-for-bit implementation identity. A result can compare the two baseline implementations; it cannot be simplified to “the languages are identical except for syntax.”
