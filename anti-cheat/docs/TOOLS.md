# Tools

## strategy_lab

Primary catalog runner for red/blue pairs.

| Item | Path |
|------|------|
| CLI | `scenarios/strategies/framework/main.cpp` → `anti-cheat/build/dev/strategy_lab` |
| Registry | `scenarios/strategies/framework/registry.cpp` |
| Framework | `scenarios/strategies/framework/` |
| Strategy sources | `scenarios/strategies/<tier>/` |
| Human catalog | `scenarios/strategies/CATALOG.md` + `docs/STRATEGY-CATALOG.md` |

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run 01_external_rpm
./anti-cheat/build/dev/strategy_lab all --quiet
```

New pair workflow:

1. Add `scenarios/strategies/<tier>/<name>/{red_example,blue_example,pair}.cpp` (+ optional `LESSON.md`).
2. Export `entry_NN_*()` from `pair.cpp` and register it in `framework/registry.cpp`.
3. Add the three sources to `cmake/ScenarioSources.cmake` so CMake compiles them into `ac_strategies`.
4. Rebuild.

## fps_demo

Plant/defuse FPS scenario + residual modes.

| Item | Path |
|------|------|
| Source | `apps/demos/fps_demo.cpp` |
| Binary | `anti-cheat/build/dev/fps_demo` |
| Library | `ac_fps` (`src/simulation/fps/`) |

```bash
./anti-cheat/build/dev/fps_demo all
```

## Narrated labs

| Binary | Source |
|--------|--------|
| `evasion_lab` | `apps/demos/evasion_lab.cpp` |
| `features_lab` | `apps/demos/features_lab.cpp` |
| `ops_lab` | `apps/demos/ops_lab.cpp` |
| `structural_lab` | `apps/demos/structural_lab.cpp` |
| `duel_t0` … `duel_t4` | `apps/demos/duel_tN.cpp` |
| `proto_tN_*` | `apps/demos/proto_tN_{red,blue}.cpp` |
| `cs2_radar_tN` | `radar/apps/demos/cs2_radar/tN/` (radar tree) |

## GUI demo (`gui_demo`)

Immediate-mode Windows GPU radar demo. No CS2 process is required because it
uses simulated entities, but the target is created only when the real RPM,
syscall, and GPU capabilities are enabled. `gui_demo` and `live_radar` live in
the radar tree, so configure and build there.

| Item | Path |
|------|------|
| Source | `radar/apps/demos/gui_demo.cpp` |
| Binary | `radar/build/dev/gui_demo` (multi-config generators: `radar/build/Release/gui_demo.exe`) |

Live CS2 radar with the same panel: `radar/build/dev/live_radar`.
| Library | `real::gpu::gui` (`radar/adapters/real/gpu/gui.hpp` + `gui.cpp`) |

```bash
cmake -S radar -B radar/build/dev -DLR_ENABLE_REAL_RPM=ON -DLR_ENABLE_REAL_SYSCALL=ON -DLR_ENABLE_REAL_GPU=ON
cmake --build radar/build/dev --target gui_demo
./radar/build/dev/gui_demo
```

Keys: `INSERT` toggles the control panel, `END`/`ESC` quits. Settings persist to `radar.ini` next to the executable.

## Live Radar Control Panel

`live_radar` embeds the same control panel. Press `INSERT` to open it while the radar overlay is running. Settings apply live and persist on exit.

Keys while the radar runs: `END` quit, `F` filter, `R` refresh layout, `D` debug ring, `I` cycle intel policy (auto → pinned hud-echo → pinned full), `B` blue score. `--full` starts pinned to all-entity intel, `--strict-intel` starts pinned to hud-echo. See [STEALTH-POSTURE.md](STEALTH-POSTURE.md).

| Tab | Settings |
|-----|----------|
| Radar | filter, radar scale, draw background/border/crosshair, enemy arrows, health rings, colors |
| ESP | enable, boxes, names, health, distance, visible-only |
| Aim | aim assist, smoothing, FOV, triggerbot + delay |
| Overlay | alpha, topmost, clickthrough |
| Misc | show FPS, show intel, show blue, save/load config |

Config is stored via `load_radar_config`/`save_radar_config` in `radar.ini` next to the executable.
