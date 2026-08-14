# Build and Test

## Configure and Build

```
cd code
cmake -S . -B build -DLR_BUILD_TESTS=ON -DLR_BUILD_PROTOS=ON -DLR_BUILD_STRATEGY_LAB=ON
cmake --build build -j
```

## Options

| Option | Default | Description |
|--------|:-------:|-------------|
| `LR_BUILD_TESTS` | ON | CTest targets under `tests/` |
| `LR_BUILD_PROTOS` | ON | Per-tier prototype demos (`proto_tN_*`), CS2 radar demos |
| `LR_BUILD_DUELS` | ON | Red-vs-blue duel demos (`duel_tN`) |
| `LR_BUILD_STRATEGY_LAB` | ON | `strategy_lab` CLI + `ac_strategies` library |
| `LR_ENABLE_REAL_UEFI` | OFF | UEFI firmware analysis backend |
| `LR_ENABLE_REAL_LINUX` | OFF | Linux live backend and portable host stubs |
| `LR_ENABLE_REAL_ALL` | OFF | Enable all real mode backends (Windows only) |

The default configuration is simulation-only on every platform. Real
backends and live-reader features require an explicit CMake opt-in.

See `CMakeLists.txt` for the full list of `LAB_ALLOW_*` flags that enable additional educational simulation surfaces.

## Directory layout

| Kind | Path (under `code/`) |
|------|------|
| Libraries | `lib/ac/`, `lib/sim/`, `lib/ac_sim/`, `lib/cs2/`, `lib/real/`, … |
| Team libraries | `teams/t0_red/` … `teams/t4_blue/` |
| Strategy pairs | `strategies/t0/` … `strategies/crosscutting/` |
| Demos | `demos/` (flat `.cpp` + `demos/cs2_radar/tN/`) |
| Tests | `tests/` |
| Project docs | `../docs/` (curriculum); `docs/` (code-adjacent notes) |

## Test

```
ctest --test-dir build --output-on-failure
./build/strategy_lab all --quiet
```

## Common Binaries

| Binary | Source | Description |
|--------|--------|-------------|
| `strategy_lab` | `strategies/framework/main.cpp` | Strategy catalog runner |
| `fps_demo` | `demos/fps_demo.cpp` | FPS game state demo |
| `radar_t0` … `radar_t4` | `demos/radar_t{0-4}.cpp` | Per-tier live radar overlays |
| `live_radar` | `demos/live_radar.cpp` | Live CS2 radar overlay (T0) |
| `tier_comparison` | `demos/tier_comparison.cpp` | All-tier timing comparison |
| `full_prototype` | `demos/full_prototype.cpp` | All backend exercise |
| `duel_t0` … `duel_t4` | `demos/duel_t{0-4}.cpp` | Red-vs-blue duels |
| `cs2_radar_t0` … `cs2_radar_t4` | `demos/cs2_radar/t{0-4}/` | Alternative CS2 radars |
