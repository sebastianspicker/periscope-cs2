# 19_input_synthesis — Synthetic / hardware input

Family: Feature. Tiers: all. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Mouse via inject or external MCU multi-sample

Blue: Input provenance multi-reason (bad source + multi/mixed)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::input_synthesis::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Synthetic input multi-step — serial_arduino + kmbox multi-sample; no open_process; residual 
2. Step 1: NO open_process / no foreign VM_READ — pure input path abuse.
3. Step 2: multi-sample push_input from serial_arduino AND kmbox (≥4 events).
4. Optional extra samples for multi_sample strength.
5. Step 3: residual mixed raw/inject path OR WH_MOUSE residual.
6. Step 4: optional ClipCursor confine scar.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- raw_sendinput_mixed — World.raw_sendinput_mixed = true
- wh_mouse_hook — World.wh_mouse_hook = true
- clipcursor_confined — World.clipcursor_confined = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.memory_clean && r.bad_sources_planted`

## BLUE

Entry: `examples::input_synthesis::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Input provenance multi-reason — Flag bad source only with multi-sample or mixed residual — not 
2. detected = bad_source && (multi_sample || mixed_raw) — not single event echo

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `bad_source`
- result field `multi_sample`
- result field `mixed_raw`
- result field `multi_reason`
- result field `detected`
- result field `bad_event_count`
- local `kMultiSampleN` init=4

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (raw_sendinput_mixed and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 19_input_synthesis
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `input_synthesis/red_example.cpp` — full red multi-step
- `input_synthesis/blue_example.cpp` — full blue multi-reason
- `input_synthesis/pair.cpp` — StrategyEntry wiring + narrator
