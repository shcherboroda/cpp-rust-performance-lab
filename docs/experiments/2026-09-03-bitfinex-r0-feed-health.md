# Bitfinex R0 feed-health observation

This is an operational network observation, not a C++/Rust benchmark and not
an estimate of exchange-to-client one-way latency.

## Conditions

- Endpoint: `wss://api-pub.bitfinex.com/ws/2`
- Subscription: `tBTCUSD`, `R0`, `F0`, `len=25`
- 4 concurrent public connections; 8 seconds each
- Local WSL2 desktop runtime, 2026-09-03

## Result

| Metric | p50 | p99 | Max |
| --- | ---: | ---: | ---: |
| Connect | 121.935 ms | 140.975 ms | 140.975 ms |
| Subscribe acknowledgement | 354.091 ms | 373.954 ms | 373.954 ms |
| Snapshot | 354.126 ms | 374.779 ms | 374.779 ms |
| First market update | 552.861 ms | 571.524 ms | 571.524 ms |
| Market-update interarrival | 0.003 ms | 224.655 ms | 384.219 ms |

Every connection received 707 market updates (2,824 total observations).

## Interpretation

The very small p50 interarrival and much larger tail show that updates are
observed in bursts by the local runtime. They do not reveal the latency from an
exchange matching engine to this host: neither synchronized clocks nor a
per-update exchange send timestamp is available. The feed receiver therefore
records interarrival, callback-to-handoff time, frame class, and queue-failure
counters separately; offline replay remains the comparison boundary.
