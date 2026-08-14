# Tools

## strategy_lab

Primary catalog runner for red/blue pairs.

| Item | Path |
|------|------|
| CLI | `code/strategies/framework/main.cpp` → `./build/strategy_lab` |
| Registry | `code/strategies/framework/registry.cpp` |
| Framework | `code/strategies/framework/` |
| Strategy sources | `code/strategies/<tier>/` |
| Human catalog | `code/strategies/CATALOG.md` + `docs/STRATEGY-CATALOG.md` |

```bash
./build/strategy_lab list
./build/strategy_lab run 01_external_rpm
./build/strategy_lab all --quiet
```

New pair workflow:

1. Add `code/strategies/<tier>/<name>/{red_example,blue_example,pair}.cpp` (+ optional `LESSON.md`).
2. Export `entry_NN_*()` from `pair.cpp` and register it in `framework/registry.cpp`.
3. Rebuild — CMake GLOBs `strategies/**/*.cpp` into `ac_strategies`.

## fps_demo

Plant/defuse FPS scenario + residual modes.

| Item | Path |
|------|------|
| Source | `code/demos/fps_demo.cpp` |
| Binary | `./build/fps_demo` |
| Library | `ac_fps` (`lib/fps/`) |

```bash
./build/fps_demo all
```

## Narrated labs

| Binary | Source |
|--------|--------|
| `evasion_lab` | `demos/evasion_lab.cpp` |
| `features_lab` | `demos/features_lab.cpp` |
| `ops_lab` | `demos/ops_lab.cpp` |
| `structural_lab` | `demos/structural_lab.cpp` |
| `duel_t0` … `duel_t4` | `demos/duel_tN.cpp` |
| `proto_tN_*` | `demos/proto_tN_{red,blue}.cpp` |
| `cs2_radar_tN` | `demos/cs2_radar/tN/` |

## GUI demo (`gui_demo`)

Immediate-mode GUI radar demo. No CS2 required (simulated entities).

| Item | Path |
|------|------|
| Source | `code/demos/gui_demo.cpp` |
| Binary | `./build/gui_demo` (multi-config generators: `build/Release/gui_demo.exe`) |

Live CS2 radar with the same panel: `./build/live_radar`.
| Library | `real::gpu::gui` (`lib/real/gpu/gui.hpp` + `gui.cpp`) |

```bash
./build/gui_demo
```

Keys: `INSERT` toggles the control panel, `END`/`ESC` quits. Settings persist to `radar.ini` next to the executable.

## Live Radar Control Panel

`live_radar` embeds the same control panel. Press `INSERT` to open it while the radar overlay is running. Settings apply live and persist on exit.

| Tab | Settings |
|-----|----------|
| Radar | filter, radar scale, draw background/border/crosshair, enemy arrows, health rings, colors |
| ESP | enable, boxes, names, health, distance, visible-only |
| Aim | aim assist, smoothing, FOV, triggerbot + delay |
| Overlay | alpha, topmost, clickthrough |
| Misc | show FPS, show intel, show blue, save/load config |

Config is stored via `load_radar_config`/`save_radar_config` in `radar.ini` next to the executable.
