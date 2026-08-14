# 43_section_map — Section / mapped entity stream

Family: Evasion. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: SharedSection carries entity bytes; optional brief open then close

Blue: Scan sections: carries_entity_bytes + non-AC creator

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::section_map::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Section / mapped view — Plant SharedSection carrying entity bytes; no long-lived VM_READ handle.
2. Optional brief open then close: red does not keep a durable VM_READ edge.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.

Achieved when: `!red_holds_handle`

## BLUE

Entry: `examples::section_map::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Section object scan — Any entity-carrying section with non-AC creator is a scar.
2. local `suspect` init=0

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `suspect` init=0

Win conditions for this pair:
- detected := `suspect > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() AccessMask::VmRead and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 43_section_map
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `section_map/red_example.cpp` — full red multi-step
- `section_map/blue_example.cpp` — full blue multi-reason
- `section_map/pair.cpp` — StrategyEntry wiring + narrator
