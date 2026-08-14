# 09_overlay_esp — Overlay ESP

Family: Feature. Tiers: T0-T1. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Draw boxes via overlay/hijack + data handle

Blue: Composition + handle correlation (not OR alone)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::overlay_esp::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Overlay ESP multi-step — Spawn esp, open VM_READ, plant topmost/transparent/hijack overlay.
2. Step 1: spawn esp process.
3. Step 2: open VM_READ handle (data path for boxes/bones).
4. Step 3: add_overlay topmost + transparent + hijacks_swapchain.
5. Step 4: optional second overlay (composition residual cluster).
6. Optional module scar on game process (present residual).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- inject_module() — Module inject (manual_map clears PEB link when true).
- spawn() — Spawn actor process on World process list.
- add_overlay() — Register OverlayWindow scar (topmost/transparent/swapchain).
- present_hooked — present_hooked=true scar

Achieved when: `r.overlay_planted && r.handle_open`

## BLUE

Entry: `examples::overlay_esp::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Composition heuristics — Topmost transparent + swapchain hijack.
2. Narrator counter: Handle correlation — Overlays need data — strong detect is overlay ∧ handle.
3. multi_reason = overlay && handle (lesson: overlays need data path)
4. reads handle graph via handles_to()
5. inspects OverlayWindow list
6. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `overlay_hit`
- result field `handle_hit`
- result field `multi_reason`
- result field `detected`
- reads handle graph via handles_to()
- inspects OverlayWindow list
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.overlay_hit && r.handle_hit`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 09_overlay_esp
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T1
```

Open the pair sources beside this lesson:

- `overlay_esp/red_example.cpp` — full red multi-step
- `overlay_esp/blue_example.cpp` — full blue multi-reason
- `overlay_esp/pair.cpp` — StrategyEntry wiring + narrator
