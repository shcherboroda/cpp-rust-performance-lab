# BBO snapshot cell

`BboSnapshotCell` is a single-writer/multi-reader publication cell for a
four-field BBO view. The order-book writer publishes after it has completed an
event. Readers use `try_read`: they receive one internally consistent snapshot
or retry when a publication overlaps their read; neither side uses a mutex.

The component is a seqlock, not a general multi-writer container. Exactly one
writer is required. A continuously publishing writer can starve readers, so it
is appropriate for small, latest-value market-data views—not for a lossless
event queue. It publishes only the BBO view and does not make the full order
book safe for concurrent access.

The C++ and Rust tests run a writer through 100,000 snapshots whose four values
obey a cross-field invariant while a concurrent reader validates every accepted
snapshot. This is a regression test, not a formal proof across all memory
models; the Acquire/Release ordering is part of the implementation contract.
