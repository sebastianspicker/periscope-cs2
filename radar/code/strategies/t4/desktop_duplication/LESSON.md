# 72_desktop_duplication — Desktop duplication residual

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: DXGI desktop dup + clean process; no OpenProcess

Blue: Multi-reason desktop_dup+clean; fog mitigate

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::desktop_duplication::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Desktop duplication capture — DXGI Desktop Duplication grabs the frame; process list stays clean.
2. Step 1: DXGI Desktop Duplication — capture compositor output.
3. Step 2: no OpenProcess / no local memory scar (process list stays clean).
4. Optional correlate: raw_hid only from user is fine; vision path needs no HID.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- desktop_duplication — World.desktop_duplication = true

Achieved when: `r.desktop_dup && r.no_game_handle && r.process_list_clean`

## BLUE

Entry: `examples::desktop_duplication::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Capture API + clean-process correlate — Flag desktop_duplication with process-clean multi-reason.
2. Narrator counter: Interest management residual — Fog still starves free enemy origin without a memory scar.
3. Multi-reason: capture API scar + process-clean (no memory reader).
4. local `sibling_capture` init=w.external_display_clone || w.trust.capture_card_present ||
      w.gdi_bitblt_capture || w.printwindow_capture
5. local `reasons` init=0
6. reads handle graph via handles_to()
7. reads HostTrust platform fields
8. structural fog / stream surfaces
9. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `desktop_dup`
- result field `process_clean`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `sibling_capture` init=w.external_display_clone || w.trust.capture_card_present ||
      w.gdi_bitblt_capture || w.printwindow_capture
- local `reasons` init=0
- reads handle graph via handles_to()
- reads HostTrust platform fields
- structural fog / stream surfaces
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.desktop_dup && (r.multi_reason || r.process_clean)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (desktop_duplication and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 72_desktop_duplication
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `desktop_duplication/red_example.cpp` — full red multi-step
- `desktop_duplication/blue_example.cpp` — full blue multi-reason
- `desktop_duplication/pair.cpp` — StrategyEntry wiring + narrator
