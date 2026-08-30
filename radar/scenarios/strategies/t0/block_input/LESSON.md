# 89_block_input — BlockInput session lock

Family: Feature. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: block_input_active=true

Blue: Detect block_input_active

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::block_input::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: BlockInput lock — BlockInput freezes user input for automation / takeover.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- block_input_active — World.block_input_active = true
- open_process() AccessMask::Query — OpenProcess-style handle scar (owner→target, access mask). AccessMask::Query
- spawn() — Spawn actor process on World process list.
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.block_input_active`

## BLUE

Entry: `examples::block_input::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Input lock / session scar — Flag block_input_active presence in the session.
2. Multi-step: flag scar + any foreign process with blocked input events.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `block_input_active`
- result field `detected`
- result field `mitigated`
- local `blocked_events` init=false
- local `helper_present` init=false

Win conditions for this pair:
- detected := `r.block_input_active || (blocked_events && helper_present)`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (block_input_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 89_block_input
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `block_input/red_example.cpp` — full red multi-step
- `block_input/blue_example.cpp` — full blue multi-reason
- `block_input/pair.cpp` — StrategyEntry wiring + narrator
