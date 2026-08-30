# 57_registry_notify_strip — Registry notify strip

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Strip CmRegisterCallback-style registry notifies

Blue: Baseline registry_notify / present; detect degradation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::registry_notify_strip::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Registry notify strip — Unregister CmRegisterCallback-style registry notifies so key-change 

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- registry_notify — World.registry_notify = 0
- registry_notify_present — World.registry_notify_present = false

Achieved when: `red.stripped`

## BLUE

Entry: `examples::registry_notify_strip::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Registry notify integrity — registry_notify count/presence vs baseline (expect ≥ 2).
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `registry_notify_degraded`
- result field `registry_notify_missing`

Win conditions for this pair:
- detected := `registry_notify_degraded || registry_notify_missing`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (registry_notify and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 57_registry_notify_strip
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `registry_notify_strip/red_example.cpp` — full red multi-step
- `registry_notify_strip/blue_example.cpp` — full blue multi-reason
- `registry_notify_strip/pair.cpp` — StrategyEntry wiring + narrator
