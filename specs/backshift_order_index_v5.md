# Backshift-deletion order index v5

## Problem

The v2–v4 fixed order-ID indexes use tombstones. A tombstone preserves lookup correctness in a linear-probing table, but it also preserves probe chains. With a long sequence of unique order IDs, a bounded table can accumulate tombstones across nearly every slot even while the live load remains low. In that state, absent-ID lookup and insertion may inspect most of the table.

This is unacceptable as an unbounded-lifetime behavior for a production component. It is not exposed by a short lifecycle trace alone.

## Isolated change

v5 defines a reusable fixed-capacity `FixedOrderIndex<Value>` for C++ and Rust:

- power-of-two capacity, at least twice maximum live entries;
- the existing SplitMix64 mixer and linear probing;
- no allocation or resizing after construction;
- an erased entry is removed with **backward-shift deletion**;
- therefore there are no tombstones in the steady state;
- every surviving entry remains reachable from its home slot before the first empty slot.

Deletion scans the following probe cluster. An entry is shifted into the current hole only when the hole lies on that entry's circular probe path from its home slot. The final hole becomes empty.

## Why this is a separate component first

The index is a reusable low-latency building block. Its own contract, tests, and long-history benchmark must be established before it replaces the index inside another order-book version. This prevents an index-policy change from being confused with changes to ladder or BBO logic.

## Required validation

1. collision-cluster lookup remains correct after deleting its head, middle, and tail;
2. repeated insert/erase histories preserve all survivors and allow new inserts;
3. no update-path allocation occurs after construction;
4. benchmark present/absent lookup, insert, and erase at controlled live loads and after long churn;
5. only after that, integrate it symmetrically into a new order-book version.
