# 39_capture_cv_hid — Capture card CV + HID

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Vision from capture card; multi-sample serial MCU — no memory scar

Blue: Multi-reason capture+input; fog residual

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::capture_cv_hid::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Capture card + CV → HID — HDMI capture on 2nd PC; multi-sample aim via Arduino/KMBox.
2. Step 1: pure vision residual — HDMI capture card on a second box.
3. Step 2: CV → aim via external HID (Arduino / KMBox). Memory path unused.
4. Step 3: process list clean — no reader, no OpenProcess, no DMA.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.capture_card_present — HostTrust.capture_card_present = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.capture_card && r.no_game_handle && r.hid_path && r.process_list_clean`

## BLUE

Entry: `examples::capture_cv_hid::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Device + input multi-reason — Capture card inventory correlated with serial/HID path class.
2. Narrator counter: Interest management residual — Fog still helps info; synthetic input remains a scar.
3. Multi-reason: device inventory + HID provenance (+ sibling capture APIs).
4. local `bad_inputs` init=0
5. local `sibling` init=w.desktop_duplication || w.external_display_clone ||
      w.gdi_bitblt_capture || w.printwindow_capture
6. local `reasons` init=0
7. reads HostTrust platform fields
8. structural fog / stream surfaces

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `capture_card`
- result field `bad_input`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `bad_inputs` init=0
- local `sibling` init=w.desktop_duplication || w.external_display_clone ||
      w.gdi_bitblt_capture || w.printwindow_capture
- local `reasons` init=0
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `(r.capture_card || r.bad_input) && (r.multi_reason || r.capture_card || r.bad_input)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (trust.capture_card_present and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 39_capture_cv_hid
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `capture_cv_hid/red_example.cpp` — full red multi-step
- `capture_cv_hid/blue_example.cpp` — full blue multi-reason
- `capture_cv_hid/pair.cpp` — StrategyEntry wiring + narrator
