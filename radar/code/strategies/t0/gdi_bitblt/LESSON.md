# 76_gdi_bitblt — GDI BitBlt screen capture

Family: Feature. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: gdi_bitblt_capture=true; no OpenProcess to game

Blue: Detect gdi_bitblt_capture; fog mitigate optional

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::gdi_bitblt::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: GDI BitBlt capture — BitBlt/PrintWindow grabs the frame; no game OpenProcess.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- gdi_bitblt_capture — World.gdi_bitblt_capture = true
- spawn() — Spawn actor process on World process list.

Achieved when: `r.gdi_capture && r.no_game_handle`

## BLUE

Entry: `examples::gdi_bitblt::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Capture API / session scar — Flag gdi_bitblt_capture presence in the session.
2. Narrator counter: Interest management residual — Fog still starves free enemy origin without a memory scar.
3. Optional structural residual: fog starves free enemy origin.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `gdi_capture`
- result field `detected`
- result field `mitigated`
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.gdi_capture`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (gdi_bitblt_capture and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 76_gdi_bitblt
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `gdi_bitblt/red_example.cpp` — full red multi-step
- `gdi_bitblt/blue_example.cpp` — full blue multi-reason
- `gdi_bitblt/pair.cpp` — StrategyEntry wiring + narrator
