# 85_external_clone_display — External clone display residual

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: external_display_clone + clean process; no OpenProcess

Blue: Multi-reason clone+clean; fog mitigate

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::external_clone_display::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: External clone display — Clone / capture path; process list stays clean; no game OpenProcess.
2. Step 1: external display clone / HDMI splitter residual.
3. Step 2: no OpenProcess — process list stays clean (off-box vision path).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- external_display_clone — World.external_display_clone = true

Achieved when: `r.external_clone && r.no_game_handle && r.process_list_clean`

## BLUE

Entry: `examples::external_clone_display::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Display topology + clean-process correlate — Flag external_display_clone with process-clean multi-reason.
2. Narrator counter: Interest management residual — Fog still starves free enemy origin without a memory scar.
3. local `sibling_capture` init=w.desktop_duplication || w.trust.capture_card_present ||
      w.gdi_bitblt_capture || w.printwindow_capture
4. local `reasons` init=0
5. reads handle graph via handles_to()
6. reads HostTrust platform fields
7. structural fog / stream surfaces
8. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `external_clone`
- result field `process_clean`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `sibling_capture` init=w.desktop_duplication || w.trust.capture_card_present ||
      w.gdi_bitblt_capture || w.printwindow_capture
- local `reasons` init=0
- reads handle graph via handles_to()
- reads HostTrust platform fields
- structural fog / stream surfaces
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.external_clone && (r.multi_reason || r.process_clean)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (external_display_clone and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 85_external_clone_display
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `external_clone_display/red_example.cpp` — full red multi-step
- `external_clone_display/blue_example.cpp` — full blue multi-reason
- `external_clone_display/pair.cpp` — StrategyEntry wiring + narrator
