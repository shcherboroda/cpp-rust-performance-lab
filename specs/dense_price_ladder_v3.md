# Dense price ladder v3

## Hypothesis

For a narrow, known price range, replacing sorted price-level vectors with a dense contiguous ladder should reduce level lookup and insertion/removal work, improving locality and reducing tail latency. The same change is applied to C++ and Rust before comparing either result to v2.

## Constant properties carried from v2

- 40-byte event representation;
- fixed 65,536-slot linear-probing order-ID index;
- SplitMix64 ID mixer and tombstone policy;
- capacity for 32,768 live orders;
- pre-generated 65,536-event lifecycle trace;
- identical timing and alternating-process protocol.

## Changed property

Each side owns an array of 256 `u64` quantities, indexed by `price_ticks - 99_873`. A zero quantity means unoccupied. The accepted inclusive range is `99_873…100_128`; an event outside it is invalid.

Best bid/ask is cached as an optional price. When an update removes the cached best level, the implementation scans the ladder in the appropriate direction to find the next occupied level. This scan is part of the measured operation.

The ladder consumes fixed memory per side and rejects prices outside its range. It is therefore a deliberately workload-specific optimization, not a replacement for the general v2 sorted-vector representation.

## Expected evaluation

Compare v3 against v2 separately in C++ and Rust:

1. Does the same structural change reduce p50/p99 in each language?
2. Does it alter the relative C++/Rust result?
3. Are gains preserved when the profile later widens the range or raises the level count?

Do not call the dense ladder an improvement until it is measured against v2 under the same protocol.
