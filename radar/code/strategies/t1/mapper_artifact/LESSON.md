# 66_mapper_artifact — Mapper process artifact

Family: Delivery. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Spawn kdmapper/mapper.exe; set mapper_process_present; unsigned path

Blue: Detect mapper_process_present or process name contains mapper/kdmapper

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::mapper_artifact::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Mapper process artifact — Spawn kdmapper.exe; set mapper_process_present; load unsigned path.
2. Optional co-artifact: mapper loads an unsigned driver image path.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- mapper_process_present — World.mapper_process_present = true
- Driver.name — Driver.name = "mapped.sys"
- Driver.sha256 — Driver.sha256 = "unsigned_mapper_payload"
- Driver.signer — Driver.signer = "unsigned"
- Driver.boot_start — Driver.boot_start = false
- Driver.byovd_known_bad — Driver.byovd_known_bad = false
- Driver.is_ac — Driver.is_ac = false
- Driver.is_bridge — Driver.is_bridge = false
- Driver.provides_mem_rw — Driver.provides_mem_rw = true
- Driver.load_order — Driver.load_order = 90
- load_driver() — Load Driver into World.drivers (kernel image scar).
- spawn() — Spawn actor process on World process list.

Achieved when: `w.mapper_process_present && out.actor_pid != 0 && out.unsigned_path_loaded`

## BLUE

Entry: `examples::mapper_artifact::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Mapper name / flag — Detect mapper_process_present or process name mapper/kdmapper.
2. Sensor 1: world scar flag set by known mapper tooling.
3. Sensor 2: process name contains mapper / kdmapper.
4. local `flag_hit` init=w.mapper_process_present
5. local `name_hits` init=0

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `flag_hit` init=w.mapper_process_present
- local `name_hits` init=0

Win conditions for this pair:
- detected := `flag_hit || name_hits > 0`
- mitigated := `out.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (mapper_process_present and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 66_mapper_artifact
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `mapper_artifact/red_example.cpp` — full red multi-step
- `mapper_artifact/blue_example.cpp` — full blue multi-reason
- `mapper_artifact/pair.cpp` — StrategyEntry wiring + narrator
