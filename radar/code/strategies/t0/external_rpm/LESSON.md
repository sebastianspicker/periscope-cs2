# 01_external_rpm — External usermode RPM

Family: Delivery. Tiers: T0. Area: t0.

## Battlefield

Red: OpenProcess + ReadProcessMemory from another process

Blue: Enumerate handles with VM_READ into the game

Arena: sim::World (default) OR real CS2 process (dual mode).

## Dual mode (educational)

This strategy runs in two modes for comparison:

| Aspect | SIM mode | REAL mode |
|--------|----------|-----------|
| Target | sim::World (synthetic) | Real cs2.exe process |
| Handle | Simulated handle graph | Real OpenProcess(PROCESS_VM_READ) |
| Entity read | Synthetic entity table | Real CS2 entity list from process memory |
| Detection | Blue sensors scan World fields | Blue simulation: handle opened + RPM telemetry |
| Game required | No | Yes (cs2.exe must be running) |

Run in real mode:
```bash
./build/strategy_lab run 01_external_rpm    # Hybrid: tries real CS2 + sim
LR_MODE=real ./build/strategy_lab run 01_external_rpm  # Force real only
```

## RED

Entry: `examples::external_rpm::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: External RPM — OpenProcess(VM_READ) from a separate process; no inject.
2. Full shipped T0 client path — not a one-liner open_process.

Team / depth APIs used:
- `t0_red::CheatClient`
- call `client.run_full_loop()`
- call `client.pid()`

Expanded team path (what the wrapper actually does on sim::World):
- Spawn cheat process (world.spawn) with a chosen process name.
- attach_to_game: plant lab entities if needed, then RpmBackend.attach_world — leaves a foreign VM_READ handle on the game pid.
- pull_entities: EntityPipeline.refresh reads the entity table through the RPM backend.
- render_radar: project entities to blips; optional RadarUi.present_external_window registers an external overlay scar.
- Optional weak evasion (does not erase the handle graph): disguise_name, hide_from_weak_process_enum, throttle_mark, close_and_reopen_brief (hidden_during_enum + brief_reopen).

World scars and lab surfaces (from shipped red code):
- Handle graph: foreign VM_READ (owner=cheat pid → target=game pid) via RpmBackend.attach_world.
- Entity table bytes on game process (plant_lab_entities + EntityPipeline.refresh).
- Optional OverlayWindow from RadarUi.present_external_window.
- Optional Handle.hidden_during_enum + brief_reopen; Process.hidden_from_weak_enum.

Achieved when: `rep.attached && rep.entities_ok && rep.entity_count > 0`

## BLUE

Entry: `examples::external_rpm::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Handle graph — Any non-AC process with VM_READ to game is a scar.
2. Full shipped T0 agent (const world → use local copy for scan that only reads).

Team / depth sensors:
- `t0_blue::AcAgent`
  - full_scan merges independent sensors (not a single bool):
  -   scan_handles — foreign VM_READ on game pid (primary scar).
  -   scan_cooccurrence — suspicious reader names / unnamed co-runners.
  -   scan_injection — foreign thread / manual map on game process.
  -   scan_module_integrity — .text / IAT/EAT / Present hooks.
  -   scan_overlays — external topmost overlay windows.
- `ac::MemoryTelemetrySink`
- call `agent.full_scan()`

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `d.handle_hit || d.cooccurrence_hit`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

External RPM is won on the handle graph. Renames and packing do not remove VM_READ. Red's next step is T1 syscall soft or T2 no-handle kernel paths — not better strings.

## Run

```bash
./build/strategy_lab run 01_external_rpm
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `external_rpm/red_example.cpp` — full red multi-step
- `external_rpm/blue_example.cpp` — full blue multi-reason
- `external_rpm/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t0/red/cheat_client.hpp`
- `t0/blue/ac_agent.hpp`
