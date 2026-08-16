# Limit Order Book - Matching Engine

A price-time priority limit order book and matching engine in C++17, built in
two iterations: a straightforward STL implementation (v1), then a
memory-pooled rewrite (v2) after profiling showed allocator traffic dominating
tail latency.

## Matching semantics (both versions)

- **Price-time priority:** bids sorted highest-first, asks lowest-first
  (`std::map`); FIFO within each price level.
- **Matching:** an incoming order walks the opposite book from the best price
  while prices cross, filling resting orders oldest-first. Trades execute at
  the **resting** order's price (price improvement for the aggressor), which
  is standard exchange behavior.
- **Cancel:** O(1) via an `order_id -> location` index; no book scan.
- **Complexity:** O(log P) add (P = price levels touched), O(1) cancel.

## v1 → v2: killing the allocation tail

**v1** stores each resting order as a `std::list` node, meaning one heap
allocation per resting order and one free per fill/cancel. Correct and simple, but the
per-order latency distribution had a fat tail: p99.9 around 3µs and worst-case
spikes in the *milliseconds*, both driven by malloc/free and the page faults
they trigger.

**v2** removes the allocator from the hot path entirely:

- Orders live in one **preallocated `std::vector<Node>` pool** with a
  free-list; fills and cancels recycle indices instead of freeing memory.
- Price levels hold an **intrusive doubly linked list** (head/tail indices
  into the pool) instead of `std::list`, keeping the same FIFO and O(1) mid-queue
  cancel, zero per-node allocations, better cache locality (32-byte nodes
  contiguous in memory).
- The trade output buffer is **caller-owned and reused** across calls, so
  reporting fills doesn't allocate either.
- The id→location map is `reserve()`d up front to avoid rehash spikes.

### Measured results (1M orders, single thread, same synthetic workload, same machine)

| Metric | v1 (std::list) | v2 (pooled) | Change |
|---|---|---|---|
| Throughput | ~4.1M orders/sec | ~6.85M orders/sec | **+65%** |
| Mean latency | ~201 ns | ~110 ns | −45% |
| p50 | ~141 ns | ~95 ns | −33% |
| p99 | ~615 ns | ~265 ns | **−57%** |
| p99.9 | ~3.0 µs | ~1.6 µs | −47% |
| Max | ~12–13 **ms** | ~60–160 **µs** | **~99% smaller worst case** |

The max-latency change is the headline: v1's worst case was a
12-millisecond allocation/page-fault stall, five orders of magnitude above
its median. v2's worst case stays within ~2 orders of magnitude of median.
In latency-sensitive systems the tail is the product; averages hide exactly
this.

Numbers above are from a Linux container (g++ 13, -O2). **Re-run
`make bench` on your own hardware before quoting them** since absolute values
vary by machine; the v1→v2 relative improvement is the durable claim.

## Correctness

Both versions share a test suite covering: basic matching, price-time
priority, price improvement, non-crossing orders resting, cancel semantics,
multi-level sweeps, plus v2-specific tests for free-list node reuse and
mid-queue intrusive-list cancellation (the two easiest things to get wrong
in a pooled design).

```
make test      # runs both suites
```

## Benchmarks

```
make bench     # v1-only detailed run
make compare   # v1 vs v2 head-to-head on identical order flow
```

## Possible extensions

- Market orders, stop orders, order modification (cancel-replace)
- Fixed-point integer prices (production books don't use doubles)
- A thin TCP/UDP order-entry gateway simulating a real feed
- Single-writer matching thread with lock-free ingestion queues (typical
  exchange architecture)

## Layout

```
include/order_book.hpp      v1 interface
include/order_book_v2.hpp   v2 interface (pooled)
src/order_book.cpp          v1 implementation
src/order_book_v2.cpp       v2 implementation
src/demo_main.cpp           small walkthrough demo
tests/                      correctness suites for both
bench/benchmark.cpp         v1 detailed benchmark
bench/benchmark_compare.cpp v1 vs v2 comparison
```
