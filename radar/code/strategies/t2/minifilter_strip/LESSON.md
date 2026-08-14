# 56_minifilter_strip — Minifilter strip

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Strip FsRtl/FltMgr minifilter callbacks (path sensors)

Blue: Baseline minifilter_callbacks / present; detect degradation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::minifilter_strip::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Minifilter strip — Unregister FsRtl/FltMgr-style file minifilter callbacks so path 

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- minifilter_callbacks — World.minifilter_callbacks = 0
- minifilter_present — World.minifilter_present = false

Achieved when: `red.stripped`

## BLUE

Entry: `examples::minifilter_strip::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Minifilter integrity — minifilter_callbacks count/presence vs baseline (expect ≥ 2).
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `minifilter_callbacks_degraded`
- result field `minifilter_missing`

Win conditions for this pair:
- detected := `minifilter_callbacks_degraded || minifilter_missing`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (minifilter_callbacks and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 56_minifilter_strip
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `minifilter_strip/red_example.cpp` — full red multi-step
- `minifilter_strip/blue_example.cpp` — full blue multi-reason
- `minifilter_strip/pair.cpp` — StrategyEntry wiring + narrator
