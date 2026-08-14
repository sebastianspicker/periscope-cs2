# 91_raw_vs_sendinput — Raw HID vs SendInput mix

Family: Feature. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: raw_sendinput_mixed=true; push raw_hid + injected

Blue: Detect raw_sendinput_mixed or mixed sources in inputs

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::raw_vs_sendinput::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Raw HID + SendInput mix — Set raw_sendinput_mixed; push raw_hid and injected inputs.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- raw_sendinput_mixed — World.raw_sendinput_mixed = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `out.flag_set && out.raw_count > 0 && out.injected_count > 0`

## BLUE

Entry: `examples::raw_vs_sendinput::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Input provenance mix — Detect raw_sendinput_mixed or mixed sources in inputs.
2. Sensor A: explicit mixed-source scar flag.
3. Sensor B: provenance inventory — raw HID + injected in same window.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `flag_hit`
- result field `mixed_sources`
- result field `detected`
- result field `mitigated`
- result field `raw_count`
- result field `injected_count`

Win conditions for this pair:
- detected := `out.flag_hit || out.mixed_sources`
- mitigated := `out.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (raw_sendinput_mixed and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 91_raw_vs_sendinput
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `raw_vs_sendinput/red_example.cpp` — full red multi-step
- `raw_vs_sendinput/blue_example.cpp` — full blue multi-reason
- `raw_vs_sendinput/pair.cpp` — StrategyEntry wiring + narrator
