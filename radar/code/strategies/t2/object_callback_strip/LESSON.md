# 47_object_callback_strip — Object callback strip

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Strip Ob* handle callbacks (not process notify)

Blue: Baseline object_callbacks / present; detect degradation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::object_callback_strip::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Object callback strip — Clear ObRegisterCallbacks-style handle create hooks (not process 

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- object_callbacks_true — World.object_callbacks_true = 0
- object_callbacks_true_present — World.object_callbacks_true_present = false
- object_callbacks — World.object_callbacks = 0
- object_callbacks_present — World.object_callbacks_present = false

Achieved when: `red.stripped`

## BLUE

Entry: `examples::object_callback_strip::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Object callback integrity — object_callbacks count/presence vs baseline (not process_notify).
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `object_callbacks_degraded`
- result field `object_callbacks_missing`

Win conditions for this pair:
- detected := `object_callbacks_degraded || object_callbacks_missing`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (object_callbacks_true and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 47_object_callback_strip
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `object_callback_strip/red_example.cpp` — full red multi-step
- `object_callback_strip/blue_example.cpp` — full blue multi-reason
- `object_callback_strip/pair.cpp` — StrategyEntry wiring + narrator
