# 162_dirty_bitmap_invalidation - Dirty-bitmap selective invalidation vs broadcast-storm observation

Family: Structural. Tiers: T3. Area: t3. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read bursts force the validator into broadcast (global) invalidation storms; the storm cadence is observable system-wide, so red infers when and where blue scans

Blue: Detect that the broadcast storm leaks scan-cadence intel to a foreign observer, and mitigate by adopting dirty-bitmap selective invalidation - only flagged consumers flush

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dirty_bitmap_invalidation::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process.
2. Spawn a `storm-observer.exe` observer (`storm_observer_pid`).
3. Read bursts: each foreign burst forces `kEventsPerBurst` global invalidation events - the lab collapses "blue scan + global flush" into `global_invalidation_events`.
4. Storm: past the threshold, `broadcast_invalidation_storm` is set - a global broadcast is an observable system event.
5. Observation: `storm_observation_active`, `storm_observations` = event count; red infers blue's scan schedule from the cadence.

Real-world mechanism mapped to lab scars:

- A validator that answers any dirty page with a *global* TLB/validator
  flush (IPI broadcast, full INVEPT, cache-line broadcast) emits a system-wide
  signal synchronized with its own scanning. Red times those broadcasts to
  map blue. `global_invalidation_events`, `broadcast_invalidation_storm`, and
  `storm_observation_active` model the observable leak and the observer.

Achieved when: a storm occurred and red inferred the scan cadence from it.

## BLUE

Entry: `examples::dirty_bitmap_invalidation::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (the leak): the broadcast storm happened - blue's own flush behavior is an observable event.
2. Reason 2 (strategy scar): a foreign observer correlated the storm cadence (`storm_observations`).
3. Reason 3: foreign read bursts preceded the storm (remote-read telemetry).
4. Reason 4: cadence inference means red knows blue's scan schedule.
5. Mitigation: adopt dirty-bitmap selective invalidation - `dirty_bitmap_invalidation_active`, only `dirty_flagged_consumers` flush (`dirty_selective_flushes`), so cadence no longer broadcasts; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && dirty-bitmap adopted && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Global invalidation is simple but loud: every broadcast is a beacon that says
"blue just scanned here, just now". A dirty-bitmap design (flush only the
consumers actually holding stale data) removes the beacon - the same
structural lesson as interest management: scope your side effects to who
needs them.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 162_dirty_bitmap_invalidation
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `dirty_bitmap_invalidation/red_example.cpp` - full red multi-step
- `dirty_bitmap_invalidation/blue_example.cpp` - full blue multi-reason
- `dirty_bitmap_invalidation/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
