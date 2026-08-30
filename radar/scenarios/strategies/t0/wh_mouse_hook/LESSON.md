# 77_wh_mouse_hook — WH_MOUSE / mouse hook scar

Family: Feature. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: wh_mouse_hook=true; optional inject inputs

Blue: Detect wh_mouse_hook

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::wh_mouse_hook::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: WH_MOUSE hook — Install global mouse hook; optional injected aim deltas.
2. Optional: synthesize aim deltas via injected path (common aim-assist surface).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- wh_mouse_hook — World.wh_mouse_hook = true
- spawn() — Spawn actor process on World process list.
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.mouse_hook`

## BLUE

Entry: `examples::wh_mouse_hook::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Hook chain / session scar — Flag wh_mouse_hook presence (inputs corroborate).
2. Primary sensor is the hook scar; inputs are corroborating only.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `mouse_hook`
- result field `injected_inputs`
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `r.mouse_hook`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (wh_mouse_hook and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 77_wh_mouse_hook
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `wh_mouse_hook/red_example.cpp` — full red multi-step
- `wh_mouse_hook/blue_example.cpp` — full blue multi-reason
- `wh_mouse_hook/pair.cpp` — StrategyEntry wiring + narrator
