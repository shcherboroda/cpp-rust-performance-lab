# ADR 0001: use complementary public L3 sources

## Status

Accepted.

## Context

The first research component is an L3 order-book state engine. It needs both deterministic full-lifecycle traces for reproducible validation and a source of live public order-level updates that does not require an account or paid market-data subscription.

Bybit's public orderbook stream is aggregated L2 price/size data, so it cannot calibrate an individual-order L3 lifecycle.

## Decision

Use two sources with distinct roles:

- **Nasdaq TotalView-ITCH official sample files** provide offline, deterministic, full-lifecycle reference traces. The sample protocol has the required add, execution, cancel, delete, and replace semantics.
- **Bitfinex Raw Books (`R0`)** provide optional live, public, no-registration order-level capture. A Raw Books snapshot/update includes an order identifier, price, and signed amount; `price = 0` removes that order.

Both are normalized outside the order-book-engine timing boundary. The engine and benchmark contract remain venue-neutral.

## Consequences

- A Bitfinex update reveals an individual order state change but not its business cause. An adapter must not invent an `Execute` or `Cancel` label when the source does not supply one. Such updates are represented as source-neutral order upsert/delete transitions in a Bitfinex trace family.
- Bitfinex Raw Books are a bounded raw-book view: its documented subscription length is limited to 1, 25, 100, or 250 orders. It is useful for live order-level update profiles, but is not claimed to be a complete deep-book source.
- Nasdaq sample files are not a replacement for continuously available free historical data. Their role is validation and repeatable benchmark input, not a claim about current market activity.
- Bybit remains a candidate future L2 adapter and experiment, not an L3 source.

## Evidence

- [Nasdaq ITCH FAQ](https://classic.nasdaqtrader.com/Content/TechnicalSupport/FAQs/ITCH_FAQ.pdf) links to official raw sample files.
- [Bitfinex Raw Books documentation](https://docs.bitfinex.com/reference/ws-public-raw-books) defines the public `R0` subscription, order IDs, snapshots, updates, and removal semantics.
- [Bybit orderbook documentation](https://bybit-exchange.github.io/docs/v5/websocket/public/orderbook) defines its public stream as price/size snapshot and delta updates.
