# Order-book benchmark matrix

## Comparison rule

Every retained implementation version runs every workload its semantics support. Values are comparable only inside one workload, input profile, timed boundary, build configuration, and measurement protocol. Unsupported work is `N/A` with a reason, never silently omitted.

Inputs are pre-generated; books are preallocated; a digest is checked after the timed region. Input generation, validation, allocation, formatting, and result aggregation stay outside timing unless named as the subject.

## A. End-to-end event processing

| ID | Timed boundary | Purpose |
| --- | --- | --- |
| `stream_mixed_lifecycle` | Apply the versioned deterministic valid L3 mix. | Primary update-engine throughput and distribution. |
| `stream_mixed_with_bbo` | Apply that stream and read BBO after every event. | Cost when downstream logic needs immediate BBO. |
| `stream_bursty` | Apply the same mix in bursts separated by untimed pauses. | Cache-warmth and burst-shape sensitivity. |

Each stream runs with hot levels (many orders per price) and sparse levels (frequent level creation/removal). Event percentages and depth distributions are versioned alongside its fixture.

The first operation-coverage stream is [`stream_mixed_lifecycle_v1.md`](stream_mixed_lifecycle_v1.md). Its initial empty-at-cycle-boundary design deliberately prevents index-history effects from being conflated with order-book event semantics.

## B. Isolated valid operations

The operation, including its required ID lookup, is timed. Setup/reset is not.

| ID | Operation and state | Value |
| --- | --- | --- |
| `op_add_new` | New ID at an existing, then a new, level. | Separates index insertion from level activation. |
| `op_cancel_partial` | Reduce non-best and best orders without removal. | Lookup and quantity update. |
| `op_execute_partial` | Partial execution without removal. | Common matching update path. |
| `op_execute_full` | Full execution at non-best and best levels. | Tombstone, level removal, BBO recovery. |
| `op_delete` | Venue delete at non-best and best levels. | Full removal separate from execution semantics. |
| `op_replace` | Same-price replacement, then side/price change. | Atomic remove-plus-add and transitions. |
| `op_upsert` | Absent-ID insert and present-ID update. | Normalized source-neutral feed support. |

Invalid/error paths receive correctness tests and a separate robustness report; they do not contaminate critical-path latency distributions.

## C. Read/search operations

| ID | Timed boundary | Value |
| --- | --- | --- |
| `lookup_bbo_steady` | Read bid and ask from a stable book. | Steady BBO read cost. |
| `lookup_bbo_after_best_removal` | Remove best then read BBO. | Recovery; current `best_level_churn`. |
| `lookup_level_by_price` | Read present and absent prices. | Price-addressing and miss behavior. |
| `lookup_order_id` | Find present and absent IDs at controlled load. | Probe behavior; diagnostic API only. |

Search profiles vary index occupancy, tombstone density, and collision patterns. Those values are reported with every result.

## D. Resource and implementation properties

- `construction_and_reserve`: construction/capacity cost, separate from updates.
- `resident_layout`: object size, allocated bytes, capacity, bytes/live order.
- `allocation_audit`: allocations after construction; target is zero on update.
- `index_health`: probe distribution/tombstones in a separate instrumented build.
- `codegen_and_pmu`: assembly and PMU counters on controlled native Linux.

## E. Measurement-system perturbation

Once uninstrumented baselines are stable, repeat A–C with no hooks, disabled hooks, enabled minimal handoff, and complete capture. The difference is the measurement layer's perturbation budget.

## Publication summary rule

After each significant implementation or measurement milestone, add a summary to `docs/publication-notes/` with the question, controlled variables, measured observation and uncertainty, limits of the conclusion, and a decision: LinkedIn-ready, needs native-Linux confirmation, or internal-only. No summary makes a language-wide claim from one component or WSL2 desktop data.
