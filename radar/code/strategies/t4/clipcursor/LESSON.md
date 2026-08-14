# 97_clipcursor — ClipCursor confine residual

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: clipcursor_confined + HID; no game handle

Blue: Multi-reason confine+HID; fog mitigate

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::clipcursor::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ClipCursor confine + HID — Confine cursor and drive aim via serial/kmbox; no OpenProcess.
2. Step 1: ClipCursor-class confine residual (process-blind input lock).
3. Step 2: optional capture/HID path correlates confine with aim delivery.
4. Step 3: deliberately leave process list clean — no reader, no OpenProcess.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- clipcursor_confined — World.clipcursor_confined = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.confined && r.no_game_handle && r.hid_path`

## BLUE

Entry: `examples::clipcursor::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: ClipCursor + HID correlation — Flag confine residual with non-raw input multi-reason.
2. Narrator counter: Interest management residual — Fog starves free enemy origin under input-lock abuse.
3. Prefer multi-reason when HID path is present (lab always plants HID).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `confined`
- result field `bad_input`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `capture` init=w.trust.capture_card_present || w.desktop_duplication ||
                       w.external_display_clone
- local `reasons` init=0
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.confined && (r.multi_reason || r.bad_input || capture || r.confined / residual present /) || (r.confined)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (clipcursor_confined and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 97_clipcursor
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `clipcursor/red_example.cpp` — full red multi-step
- `clipcursor/blue_example.cpp` — full blue multi-reason
- `clipcursor/pair.cpp` — StrategyEntry wiring + narrator
