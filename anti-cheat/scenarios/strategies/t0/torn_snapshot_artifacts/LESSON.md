# 161_torn_snapshot_artifacts - Torn multi-read snapshots as blue-detectable artifacts

Family: Detection. Tiers: T0. Area: t0. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read entity fields in several separate transactions while the game state advances between them - cheaper per read, but the assembled records are torn (health/team/lifestate/origin from different ticks)

Blue: Validate the assembled records against plausibility invariants; torn records are artifacts no single consistent game tick could produce

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::torn_snapshot_artifacts::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its planted entity table.
2. Spawn a `torn-read-actor.exe` reader with a VM_READ handle (remote-read telemetry accrues).
3. Control read: one single-transaction decode (`read_entity_snapshots`) - a consistent snapshot.
4. Torn pass, record 1: read `alive`/`health` from generation 0; the game updates row 0 (origin zeroed, team 1->2, alive 0); then read `origin`/`team` from generation 1. The record combines "alive" with the zero origin.
5. Torn pass, record 2: catch the team byte mid-teardown (transient 0), read the cleared `alive`, and pair row 0's fields with row 1's origin (cross-entity contamination).
6. Retain the torn records on the World (`torn_read_records`) with the transaction count - blue validates exactly what red assembled.

Real-world mechanism mapped to lab scars:

- A seqlock/atomic-snapshot reader never tears; a field-by-field external
  reader does. The lab models the game's own updates with `write_mem` between
  red's transactions (the game writes its own memory, not red) and stores
  red's assembled `ac::EntitySnapshot` records for blue's invariant pass -
  the same way a server-side or heuristic check validates reported health /
  team / lifestate / origin combinations.

Achieved when: torn records were assembled across transactions and the control read succeeded.

## BLUE

Entry: `examples::torn_snapshot_artifacts::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1: implausible field combinations - per record, invalid team (not T/CT), alive entity at the zero origin, positive health with a dead lifestate.
2. Reason 2: the multi-transaction pattern - transactions >= 2x records is the torn-read signature.
3. Reason 3 (strategy scar): the torn multi-read pass is active.
4. Reason 4: remote-read telemetry from the field reads.
5. Mitigation posture: require single-transaction snapshot reads with sequence validation - red's records must be consistent or visibly stale (and staleness is detectable via generation counters, see 163_stale_chunk_cache).

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar (torn records exist)`
- mitigated := `detected && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Torn reads are not just a correctness bug - they are evidence. A record that
combines health from tick N with a lifestate from tick N+1 could not come
from one game state, so a plausibility checker needs no memory access at all
to catch the read pattern. The red fix (single-transaction snapshot reads)
is also the blue recommendation.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 161_torn_snapshot_artifacts
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `torn_snapshot_artifacts/red_example.cpp` - full red multi-step
- `torn_snapshot_artifacts/blue_example.cpp` - full blue multi-reason
- `torn_snapshot_artifacts/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
- `ac/types.hpp` (EntitySnapshot records blue validates)
