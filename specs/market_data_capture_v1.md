# Market-data capture and replay v1

## Source and observed feed

Bitfinex Raw Books `R0` is the initial live source. It is public, requires no
credentials, and identifies individual orders. Coinbase Exchange `full` is
deferred because its documented HMAC/passphrase authentication is unavailable
from the current account interface. Bybit remains a separate L2 source.

On 2026-09-02, a subscription to `wss://api-pub.bitfinex.com/ws/2` with
`{"event":"subscribe","channel":"book","symbol":"tBTCUSD","prec":"R0","freq":"F0","len":"25"}`
returned an `info` event, a `subscribed` acknowledgement with a dynamic channel
ID, one snapshot and individual updates. Snapshot entries and updates are
`[order_id, price, amount]`; positive amount is bid, negative is ask, and zero
price deletes the order. `len=25` produced 25 orders on each side.

This is order-level state, not order lifecycle: non-zero price normalizes to
`OrderUpsert`, zero price to `OrderDelete`. The adapter must never infer cancel
or execution causes. The feed has no monotonic book sequence.

## Boundaries and failure policy

```text
WebSocket -> exact raw-frame recorder -> fixed SPSC queue -> R0 decoder ->
checksum/reconnect control -> canonical events -> L3 book
                    ^
saved capture -> offline replay -+
```

The recorder stores every application-delivered frame before decoding, with
capture index, monotonic and UTC receive timestamps, connection ID, direction,
kind, exact payload bytes, length and CRC32C. Segments are atomically published
with a manifest containing SHA-256 digests, endpoint/subscription, build,
capacities and lifecycle outcomes.

The queue and payload arena are preallocated. Queue overflow, recorder failure,
parse failure, channel mismatch, disconnect or checksum mismatch invalidates
the local book and triggers reconnect. No frame is silently omitted while the
book is marked live. Reconnect discards the old book and requires a new
snapshot. Bitfinex checksum, when enabled, validates only the documented top
25 orders on each side; it is not a deep-book continuity guarantee.

## Matched implementation and measurement

C++ uses Boost.Beast/Asio with OpenSSL; Rust uses Tokio with rustls and
tokio-tungstenite. Both use the same binary capture format, fixed-point policy,
SPSC semantics, frozen capture-file digest and canonical output. Live Internet
timing is operational telemetry only, never a language comparison.

Measure separately: record validation/handoff; decode/normalize; and replay
through book application. DNS, TLS, sockets, persistence, sleeps, fixtures,
reporting and validation are outside each timed interval. Allocation audits
must show zero steady-state allocations.

## Delivery order

1. Binary record reader/writer plus corruption tests in both languages.
2. Offline R0 decoder and fixed-point normalization fixtures in both languages.
3. Live clients, reconnect state and raw capture.
4. Scripted fault/checksum tests and matched offline baseline reports.

## Evidence

- [Bitfinex Raw Books](https://docs.bitfinex.com/reference/ws-public-raw-books)
- [Bitfinex public WebSocket channels](https://docs.bitfinex.com/docs/ws-public)
- [Bitfinex checksum](https://docs.bitfinex.com/docs/ws-websocket-checksum)
