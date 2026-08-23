# Bitfinex Raw Books (`R0`) source note

## Role

Optional live, no-registration capture source for the L3 order-book experiment.

## Public subscription

Endpoint: `wss://api-pub.bitfinex.com/ws/2`

Example subscription:

```json
{"event":"subscribe","channel":"book","prec":"R0","symbol":"tBTCUSD"}
```

`R0` is the raw, order-level precision. Each trading-book entry contains `order_id`, `price`, and signed `amount`; positive amount is bid and negative amount is ask. An update whose `price` is zero removes the order. See the official [Raw Books documentation](https://docs.bitfinex.com/reference/ws-public-raw-books).

## Normalization rules

- Initial snapshot entry → `OrderUpsert`.
- Update with non-zero price → `OrderUpsert` for that identifier.
- Update with zero price → `OrderDelete` for that identifier.
- Do not infer whether a disappearance or size decrease was an execution, cancellation, or venue-specific change.

## Limitations

- The stream is a live capture source, not a free historical archive.
- The documented raw-book subscription length is bounded at 1, 25, 100, or 250 orders.
- Prices and amounts arrive as decimals; conversion to fixed-point integers and the associated rounding policy belong to the adapter contract and occur outside the core benchmark.
- Transport, JSON parsing, reconnect, sequence/recovery, and capture I/O are separate experiments and are excluded from the initial order-book-engine measurements.
