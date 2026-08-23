# Final implementation matrix — current synthetic lifecycle

All six implementations passed their applicable fixture/tests.  This matrix is
15 CPU-0-pinned rotated rounds; every round runs each C++/Rust pair in a rotated
position. Each cell has 3,000 retained samples. Raw data is local at
`results/local/final-order-book-matrix-20260823T155717Z`.

| Implementation | C++ median p50 / p99 | Rust median p50 / p99 |
| --- | ---: | ---: |
| native containers | 1.842 / 5.515 ms | 1.758 / 5.032 ms |
| parity fixed table/vector | 1.114 / 3.016 ms | 1.122 / 3.050 ms |
| dense ladder | 7.568 / 14.728 ms | 11.068 / 17.875 ms |
| compact tombstone bitmap | 1.186 / 4.113 ms | 0.965 / 2.533 ms |
| backshift bitmap | 1.208 / 3.461 ms | 0.899 / 2.634 ms |
| packed backshift bitmap | **0.726 / 2.380 ms** | **0.811 / 2.507 ms** |

On this matrix, packed backshift is the benchmark winner in both languages.
However, an earlier direct compact-tombstone/backshift series produced the
opposite C++ ordering. That conflict is material and demonstrates why WSL2
desktop results cannot establish an unconditional production winner. The
repository therefore records packed backshift as the **provisional benchmark
reference**, while compact tombstone remains the no-shift production candidate.
The choice must be revisited after direct repetition and a representative real
L3 replay with high-live-order occupancy.
