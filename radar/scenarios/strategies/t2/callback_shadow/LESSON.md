# 35_callback_shadow — Callback shadow façade

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Degrade true notifies; clean single-shot sample

Blue: Sample + compare process_notify_true / shadow flag

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::callback_shadow::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Callback shadow — Strip true notify counts, then present a clean façade on probe.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- process_notify — World.process_notify = 1
- image_notify — World.image_notify = 1
- ac_callback_present — World.ac_callback_present = false

Achieved when: `red.shadowed && blue.sample_looks_clean`

## BLUE

Entry: `examples::callback_shadow::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Callback sample + shadow check — One sample can lie; compare true counts / shadow flag.
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `sample_looks_clean`
- result field `shadow_flag`
- result field `true_degraded`
- result field `sampled_process_notify`
- result field `sampled_image_notify`
- result field `sampled_ac`

Win conditions for this pair:
- detected := `shadow_flag || true_degraded`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (process_notify and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 35_callback_shadow
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `callback_shadow/red_example.cpp` — full red multi-step
- `callback_shadow/blue_example.cpp` — full blue multi-reason
- `callback_shadow/pair.cpp` — StrategyEntry wiring + narrator
