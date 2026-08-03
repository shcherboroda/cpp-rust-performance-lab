# Benchmark sanity workloads

These preliminary workloads establish a shared methodology for equivalent C++ and Rust experiments. They are not a universal benchmark framework. Every unresolved choice is explicitly marked **TBD**.

## 1. Sequential sum

### Purpose

Measure a simple contiguous read-and-accumulate workload and validate basic loop, data-layout, and optimization assumptions.

### Input

A contiguous array of 1,048,576 unsigned 64-bit integers. Generate values with a deterministic SplitMix64 sequence seeded with `0x123456789ABCDEF0`; store each generated 64-bit value directly in the input array. Both implementations must use the same generated input and expected checksum.

The size is large enough to make timer overhead negligible and to exceed the private L1 and L2 cache capacity available to a single core on the current machine, while remaining small enough for quick repeated local runs. The array may still fit within the shared last-level cache, so this workload must not be described as a main-memory benchmark.

### SplitMix64 transition

For each generated value, update `state = (state + 0x9E3779B97F4A7C15) mod 2^64`; set `z = state`; then apply `z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9 mod 2^64`, `z = (z ^ (z >> 27)) * 0x94D049BB133111EB mod 2^64`, and emit `z ^ (z >> 31)`.

### Exact operation

Start with an unsigned 64-bit accumulator equal to zero. Visit every input element once in contiguous, increasing index order and update `accumulator = accumulator + value` using wrapping arithmetic.

### Output/checksum

The timed function must return the final unsigned 64-bit accumulator. The fixed expected checksum is `9483690369738398586` (`0x839CD625000CDB7A`). This checksum is a cross-language test vector independently verified by C++, Rust, and Python. The caller must consume and validate the returned result against it so the compiler cannot remove the work.

### Overflow semantics

Arithmetic wraps modulo 2^64 in both languages.

### Setup outside the timed region

Allocation, input generation, and correctness validation must occur outside the timed region. Warm-up policy is **TBD**.

### Work inside the timed region

Perform only the specified sequential accumulation. Benchmark repetition count and timer are **TBD**.

### Optimization hazards

The initial implementation must not use explicit SIMD, parallelism, unsafe Rust, compiler intrinsics, volatile accesses, or manual loop unrolling. Compiler auto-vectorization is allowed but must later be inspected in generated assembly. Dead-code elimination, constant folding, vectorization differences, and accidental inclusion of setup work can invalidate comparison.

### Open decisions

- Benchmark repetition count: **TBD**.
- Warm-up policy: **TBD**.
- Timer: **TBD**.
- Command-line interface: **TBD**.
- Result format: **TBD**.

## 2. Conditional sum

### Purpose

Measure a contiguous scan with data-dependent selection while keeping the same correctness contract across implementations.

### Input

A contiguous numeric sequence and a condition definition, such as a threshold, bit test, or predicate. Element type, distribution, condition, and input size are **TBD**.

### Exact operation

Visit each element once in increasing index order; add an element to the accumulator only when it satisfies the selected condition. Predicate semantics are **TBD**.

### Output/checksum

Return or consume the final conditional accumulator as a checksum. Expected-value construction and validation are **TBD**.

### Overflow semantics

The arithmetic and comparison semantics, including behavior for signed values, are **TBD** and must be equivalent.

### Setup outside the timed region

Allocate and initialize input, choose or generate the condition data distribution, and calculate expected output outside timing. Data-generation reproducibility is **TBD**.

### Work inside the timed region

Perform only the scan, condition evaluation, selected additions, and minimum required result handoff.

### Optimization hazards

Dead-code elimination, predictable or constant predicates, compiler if-conversion, vectorization, and non-equivalent branch behavior may distort results.

### Open decisions

- Predicate and element type: **TBD**.
- Match rate and input distribution: **TBD**.
- Whether branchless and branch-oriented variants are separate experiments: **TBD**.
- Repetition, warm-up, and timing policy: **TBD**.

## 3. Dependency-chain accumulation

### Purpose

Measure a deliberately serial arithmetic dependency chain, separating latency-oriented behavior from independent reduction work.

### Input

A sequence of numeric values or generated operands. Operand type, sequence length, and initialization pattern are **TBD**.

### Exact operation

Update one accumulator for every operand so each update depends on the preceding accumulator value. The update expression is **TBD**.

### Output/checksum

Return or consume the final accumulator and validate it against an expected result. Validation approach is **TBD**.

### Overflow semantics

The exact arithmetic operation and its overflow behavior are **TBD**; both implementations must preserve the same semantics.

### Setup outside the timed region

Prepare operands, initial accumulator state, and expected output outside timing. Any seed selection is **TBD**.

### Work inside the timed region

Perform the dependency-preserving accumulator updates and minimum required result handoff only.

### Optimization hazards

Dead-code elimination, algebraic reassociation, vectorization, unrolling that changes dependency properties, constant operands, and language-specific overflow assumptions require inspection.

### Open decisions

- Update expression and operand type: **TBD**.
- Initial accumulator and operand generation: **TBD**.
- Required dependency strength and assembly-inspection criteria: **TBD**.
- Repetition, warm-up, and timing policy: **TBD**.

## 4. Pointer chasing

### Purpose

Measure dependent memory-access latency through a linked traversal whose next address depends on the current load.

### Input

A collection of nodes or indices forming a traversal structure. Node representation, count, layout, allocation strategy, and permutation generation are **TBD**.

### Exact operation

Start from a defined entry point and repeatedly load the next node or index, with each step dependent on the prior result. Traversal length and cycle policy are **TBD**.

### Output/checksum

Return or consume the final visited node, index, or a traversal-derived checksum. Correctness validation is **TBD**.

### Overflow semantics

If indices or counters participate in the checksum, their arithmetic semantics are **TBD**. Pointer validity and traversal bounds must be equivalent and defined.

### Setup outside the timed region

Allocate, arrange, and validate the traversal structure; select the entry point; and establish expected output outside timing. Cache-conditioning policy is **TBD**.

### Work inside the timed region

Perform only the dependent traversal and minimum required result handoff.

### Optimization hazards

Dead-code elimination, traversal simplification, prefetching, allocator and layout differences, cache residency, page placement, and accidental independent loads can change what is measured.

### Open decisions

- Node versus index representation: **TBD**.
- Data-structure size, layout, and permutation: **TBD**.
- Cache-conditioning and memory-allocation policy: **TBD**.
- Traversal count, warm-up, and timing policy: **TBD**.
