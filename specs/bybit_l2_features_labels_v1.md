# Bybit L2 features and labels v1

The offline extractor consumes only completed LLFR v2 captures. It rebuilds the
same validated book used by replay; malformed frames, topic mismatches and
update-ID gaps fail the run rather than yielding partial rows.

Each row is emitted after a valid snapshot/delta, using exchange `cts` in ms.
Features are BBO mid, spread, microprice and bid/ask imbalance over 1, 5 and
25 levels. They contain no future observations.

For horizons 100, 500 and 1000 ms, the label uses the **first captured book
state whose `cts >= row_cts + horizon`**. It is `+1`, `0`, or `-1` according to
the future mid-price relative to current mid-price. The default threshold is
one BTCUSDT tick (`10,000,000` in 1e-8 fixed-point); it is configurable as the
third command-line argument. Moves inside that neutral band receive label `0`.
Rows without every future label are dropped. This is an exploratory direction label, not an executable
PnL label; spread, fees, latency and fill uncertainty must be evaluated later.

Train/test splits must be chronological. Capture identity, topic and extractor
version belong beside each dataset; random row splitting is prohibited.
