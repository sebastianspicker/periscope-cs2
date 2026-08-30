# 64_scattered_entity_read — Scattered entity read

Family: Evasion. Tiers: T0. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read entities in random order with timing jitter

Blue: Detect via handle + scatter pattern (harder than sequential)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::scattered_entity_read::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Scattered entity read — shuffle the entity read order with a seed derived from the actor and game pids, then add inter-read timing jitter so the access pattern does not look like a sweep.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `radar-reader.exe` actor process.
- open_process() from the reader to the game with VmRead.
- Read the entity count, shuffle the read order (std::shuffle seeded by actor ^ game_pid), and read each entity at table_base + 0x10 + index * 16.
- Set World.read_timing_jitter, World.scattered_read_pattern, World.scattered_read_count.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- read_timing_jitter — World.read_timing_jitter = true.
- scattered_read_pattern — World.scattered_read_pattern = true.
- scattered_read_count — World.scattered_read_count = entity count.
- spawn() — Spawn actor process on World process list.

Achieved when: `has_handle && scar_verified`

## BLUE

Entry: `examples::scattered_entity_read::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Scatter pattern — entity reads that are non-sequential are a behavioral scar.
2. Narrator counter: Timing jitter and the remaining VM_READ handle corroborate.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- checks World.scattered_read_pattern / scattered_read_count / read_timing_jitter

Win conditions for this pair:
- detected := `(scattered_read_pattern || vm_read_handle) && signals >= 2`
- mitigated := `risk >= 0.55` (blue flips World.ranked_access_denied)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Scattered reads are an attempt to defeat sequential-access heuristics, but they replace one pattern with another: randomized order plus jitter is itself a pattern that behavioral sensors can learn. Because the VM_READ handle remains, blue can always fall back to the handle graph when the read pattern is too subtle.

## Run

```bash
./build/strategy_lab run 64_scattered_entity_read
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `scattered_entity_read/red_example.cpp` — full red multi-step
- `scattered_entity_read/blue_example.cpp` — full blue multi-reason
- `scattered_entity_read/pair.cpp` — StrategyEntry wiring + narrator
