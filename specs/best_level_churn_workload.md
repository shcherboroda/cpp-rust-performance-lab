# Best-level churn workload

## Purpose

Measure the update path that removes the only order at the current best price,
then immediately makes the best bid and best ask observable.  The ordinary
lifecycle workload is useful for the order index and aggregate quantities, but
can hide the cost of recovering BBO after a best-level deletion.

## Trace

- 32,768 independent add/execute pairs;
- even order IDs are bids at the top ladder price (`100128`), odd IDs are asks
  at the bottom ladder price (`99873`);
- every order has a deterministic quantity in `[1, 97]`;
- each `Execute` fully removes the just-added order;
- after *every* event the benchmark reads both `best_bid` and `best_ask` and
  folds the result into a digest returned after timing.

The book contains at most one live order.  This is intentional: it makes an
empty best-level transition occur on every execute, rather than letting depth
at the same price mask it.  It is a targeted microbenchmark, not a claim about
the event mix of a particular venue.

## Comparison rule

The dense ladder v3 and bitmap ladder v4 run this exact trace, with the same
event layout, fixed order index, 256-price ladder, hashing, release settings,
sample count, and process protocol.  The only intended algorithmic difference
is v4's occupancy bitmap for recovering BBO.

## Expected cost model

v3 scans up to 256 quantity entries when its cached best level becomes empty.
v4 updates one occupancy bit on the zero/nonzero transition and finds the next
occupied price by scanning at most four non-zero `u64` words with bit-scan
operations.  The measurement determines whether that exchange is beneficial
on this compiler and CPU; it is not assumed in advance.
