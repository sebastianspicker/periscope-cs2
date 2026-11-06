# 163_stale_chunk_cache - Cache coherence under list reallocation

Family: Delivery. Tiers: T0-T1. Area: t1. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Memoize container chunk pointers once and serve entity reads from the cached chunks - fast, until the container reallocates underneath the cache

Blue: Deliberately reallocate the container (churn pressure), tag chunks with generation counters, and detect stale reads as generation mismatches

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::stale_chunk_cache::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its planted entity table.
2. Spawn a `chunk-cache-actor.exe` reader with a VM_READ handle.
3. Memoize: `chunk_cache_memoized`, `memoized_chunk_ptrs = 2`, and - the key
   anchor - `memoized_chunk_generation` records the container generation the
   cache was built at.
4. Serve entity reads from the memoized chunks (`stale_chunk_reads`), decoding
   team/lifestate bytes that are plausible while the generation still matches.
5. The cache is currently valid. Its lifetime is exactly the lesson: the
   moment blue reallocates, every memoized pointer serves stale bytes.

Real-world mechanism mapped to lab scars:

- Source-2 entity lists reallocate their backing chunks; a cheat that caches
  chunk pointers keeps reading freed/reused memory after any reallocation.
  The lab models the container as `container_list_generation` (bumped on
  every reallocation) and red's cache as `memoized_chunk_ptrs` +
  `memoized_chunk_generation` - the pair a generation-counter sensor checks.

Achieved when: memoized chunks served plausible entity reads at the memoize generation.

## BLUE

Entry: `examples::stale_chunk_cache::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Blue's move: churn pressure - `container_reallocations = 2`, `container_list_generation` bumped twice, `chunk_generation_tagged` (reallocations now bump counters).
2. Reason (strategy scar): a memoized chunk cache is in circulation, tagged with its build generation.
3. Reason: stale reads observed - `stale_generation_reads` counts memoized pointers whose generation predates the current one.
4. Reason: chunk-cache read telemetry from red's apply pass.
5. Reason: remote-read telemetry from the delivery handle.
6. Mitigation: generation counters propagated to every chunk access - stale consumers must re-validate before their reads are trusted; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar (stale reads after churn)`
- mitigated := `detected && generation counters tagged && risk >= 0.65`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Caching pointers is an optimization with an expiry date that red does not
control. Blue can flip the relationship: by reallocating deliberately and
tagging generations, churn becomes a sensor - whoever keeps reading stale
chunks announces both their cache and their read cadence.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 163_stale_chunk_cache
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `stale_chunk_cache/red_example.cpp` - full red multi-step
- `stale_chunk_cache/blue_example.cpp` - full blue multi-reason
- `stale_chunk_cache/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
- sibling lesson: `t0/torn_snapshot_artifacts/LESSON.md` (stale vs torn reads)
