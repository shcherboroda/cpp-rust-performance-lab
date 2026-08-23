# Publication assessment — default hashers changed the result

## Question

What does the initial C++ versus Rust L3 order-book baseline actually measure when each implementation uses its ecosystem defaults?

## Controlled workload

The native-container `order_book_lifecycle_churn_v1` workload applies 65,536 pre-generated events per sample: 32,768 adds followed by 32,768 full executions. Each result contains 15 alternating C++/Rust processes and 200 retained samples per process. Input generation and validation are outside the timed region.

## Observations

| Index hasher configuration | Affinity | Language | pooled p50 | pooled p99 | median process p50 |
| --- | --- | --- | ---: | ---: | ---: |
| Native defaults | unpinned | C++ | 1.980 ms | 5.733 ms | 1.943 ms |
| Native defaults | unpinned | Rust | 3.078 ms | 6.419 ms | 2.958 ms |
| Shared SplitMix64 mixer | CPU 0 | C++ | 6.465 ms | 12.062 ms | 6.298 ms |
| Shared SplitMix64 mixer | CPU 0 | Rust | 2.099 ms | 5.179 ms | 2.040 ms |
| Shared SplitMix64 mixer | unpinned | C++ | 7.055 ms | 14.515 ms | 6.726 ms |
| Shared SplitMix64 mixer | unpinned | Rust | 2.243 ms | 6.015 ms | 2.176 ms |

The native C++ index used `std::unordered_map` with its default `std::hash<u64>`; Rust used `HashMap` with its default `RandomState`. They are not the same hash algorithm or seed policy. Replacing both with the same deterministic SplitMix64 ID mixer reversed the observed ordering on this workload.

The next fixed-layout parity variant also used the same mixer and fixed order-ID table. On CPU 0 its pooled p50 was 2.849 ms in C++ and 1.876 ms in Rust; p99 was 5.155 ms and 3.539 ms respectively.

## Limits

Native-container layouts, allocators, ordered price-level containers, error mechanisms, compilers, and WSL2 execution remain different. Default and custom runs occurred in separate process series; the unpinned rows are the closest direct comparison, but are still a preliminary desktop observation.

## Publication decision

**LinkedIn-ready as a methodology post, not as a language-performance claim.** The defensible point is: before concluding that a language is faster, normalize material library choices such as hashing and verify the workload boundary. The post must state that it used synthetic data on WSL2 and that the result is scoped to this implementation and machine.
