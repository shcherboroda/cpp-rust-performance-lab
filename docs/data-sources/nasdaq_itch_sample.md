# Nasdaq TotalView-ITCH sample source note

## Role

Offline deterministic reference source for full L3 lifecycle semantics, replay tests, and trace fixtures.

## Availability and provenance

Nasdaq's official ITCH FAQ links to public raw sample files at the Nasdaq EMI sample-data location. The files are released for testing and format interpretation; their inclusion in an experiment does not imply that they describe current market activity. See the [Nasdaq ITCH FAQ](https://classic.nasdaqtrader.com/Content/TechnicalSupport/FAQs/ITCH_FAQ.pdf).

## Normalization rules

- Add Order → `Add`.
- Order Executed / Order Executed With Price → `Execute`.
- Order Cancel → `Cancel`, unless the normalized remaining quantity is zero, in which case use `Delete` only when the source model requires removal.
- Order Delete → `Delete`.
- Order Replace → `Replace`.

The adapter must follow the precise fields and sequencing rules of the versioned ITCH specification used by the sample. Input file name, file hash, specification version, parser version, conversion policy, and generated trace hash are recorded alongside every derived fixture.

## Limitations

- Sample availability is not an entitlement to redistribution. Derived committed fixtures must be small and checked against the applicable terms.
- A sample file is unsuitable as evidence for current throughput, latency, or event-distribution claims.
- Packet transport, session recovery, and binary parsing are distinct scopes from the initial core state-update benchmark.
