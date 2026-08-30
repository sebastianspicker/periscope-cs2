# 16_callback_strip — Kernel callback strip

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Tamper notify chains to blind AC

Blue: Continuous callback chain integrity

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::callback_strip::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Callback stripping — Aggressive T2: remove process/image notify so AC goes blind.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- process_notify — World.process_notify = 1
- image_notify — World.image_notify = 1
- ac_callback_present — World.ac_callback_present = false

Achieved when: `red.stripped`

## BLUE

Entry: `examples::callback_strip::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Callback integrity self-audit — Compare notify counts and AC callback presence to baseline.
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `process_notify_degraded`
- result field `image_notify_degraded`
- result field `ac_callback_missing`

Win conditions for this pair:
- detected := `process_notify_degraded || image_notify_degraded ||
           ac_callback_missing`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (process_notify and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 16_callback_strip
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `callback_strip/red_example.cpp` — full red multi-step
- `callback_strip/blue_example.cpp` — full blue multi-reason
- `callback_strip/pair.cpp` — StrategyEntry wiring + narrator
