# 65_dxgi_present_hook — DXGI Present hook scar

Family: Feature. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Set present_hooked on game.exe or client.dll; optional overlay

Blue: Any is_game module with present_hooked

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dxgi_present_hook::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DXGI Present hook — Hook Present on game module; optional overlay for frame-time ESP.
2. Prefer client.dll; fall back to game.exe.
3. Optional external overlay companion (common Present-hook product surface).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- client.dll.present_hooked — client.dll Module.present_hooked = true
- game.exe.present_hooked — game.exe Module.present_hooked = true
- spawn() — Spawn actor process on World process list.
- add_overlay() — Register OverlayWindow scar (topmost/transparent/swapchain).

Achieved when: `true after planting: Module.present_hooked=true, client.dll.present_hooked=true, game.exe.present_hooked=true — dxgi_present_hook: present_hooked=true overlay_pid= (DXGI Present / swapchain scar)`

## BLUE

Entry: `examples::dxgi_present_hook::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Present / swapchain integrity — Any is_game module with present_hooked.
2. local `hits` init=0
3. inspects Process.modules (IAT/EAT/text_hash/present_hooked)
4. checks Module.present_hooked

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `hits` init=0
- inspects Process.modules (IAT/EAT/text_hash/present_hooked)
- checks Module.present_hooked

Win conditions for this pair:
- detected := `hits > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (client.dll.present_hooked and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 65_dxgi_present_hook
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `dxgi_present_hook/red_example.cpp` — full red multi-step
- `dxgi_present_hook/blue_example.cpp` — full blue multi-reason
- `dxgi_present_hook/pair.cpp` — StrategyEntry wiring + narrator
