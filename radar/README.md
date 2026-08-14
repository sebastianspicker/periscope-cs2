# AC Research Lab

Educational red/blue lab for studying **external “legit” radar** — tools that read game memory and draw a map without aiming for you — and the detection techniques that push back.

This is the **radar** track of [Periscope](../README.md) (`periscope/radar`). Sister track: [`../vision/`](../vision/) (pixel-based outlining; separate stack and license). Not a cheat product and not a production anti-cheat.

Two ways to run it:

| Mode | What it does | Needs |
|------|----------------|-------|
| **Simulation** | Red plants scars on a deterministic `sim::World`; blue reasons over them | C++20 + CMake. Any OS. |
| **Real (Windows)** | Attach to a live process (CS2 for T0/T1), read entities, draw a radar overlay | VS/MSVC or Clang-CL, optional WDK / VMX / DMA hardware for higher tiers |

Default builds use `sim::World`. Real Windows backends are optional via CMake.

---

## Why it exists

Commercial “looks legit” radar packs escalate delivery as blue closes lower scars:

```
T0  OpenProcess + ReadProcessMemory
T1  Same handle, indirect / direct syscalls (hooks are theater)
T2  Kernel / BYOVD / memrw device (no game handle)
T3  Personal hypervisor + thin bridge
T4  Off-box DMA / capture residuals
```

Each tier has a red team library, a blue team library, runnable demos, and strategy pair lessons (apply → detect) under `code/strategies/{t0..t4,crosscutting}/`.

---

## Layout

```
.
├── README.md                 You are here
├── docs/                     Curriculum and reference
├── code/                     Everything that builds
│   ├── CMakeLists.txt
│   ├── lib/                  Shared libraries
│   │   ├── ac/               Core types
│   │   ├── sim/              sim::World (scar arena)
│   │   ├── ac_sim/           Temporal / behavioral helpers
│   │   ├── server/           Interest mgmt, info-advantage, bans
│   │   ├── depth/            Multi-signal scoring
│   │   ├── fps/              FPS scenario machine
│   │   ├── lab/              Fixture process, scanners
│   │   ├── cs2/              CS2 models & signatures
│   │   ├── blue/             Blue coordinator
│   │   └── real/             OS/hardware backends (T0–T4)
│   ├── teams/                t0_red … t4_blue
│   ├── strategies/           Red/blue pair lessons + strategy_lab
│   ├── demos/                radar_tN, live_radar, duels, probes
│   ├── drivers/              Example vulnerable driver (lab only)
│   ├── firmware/             Example PCIe DMA host sim
│   └── tests/                CTest suites
```
---

## Build

### Simulation only (any platform)

```bash
cd code
cmake -S . -B build -DLR_BUILD_TESTS=ON -DLR_BUILD_STRATEGY_LAB=ON
cmake --build build -j
```

### Real backends (Windows)

```bash
cd code
cmake -S . -B build -DLR_ENABLE_REAL_ALL=ON
cmake --build build -j
```

Useful CMake flags:

| Flag | Default | Meaning |
|------|:-------:|---------|
| `LR_BUILD_TESTS` | ON | CTest targets |
| `LR_BUILD_STRATEGY_LAB` | ON | `strategy_lab` CLI |
| `LR_BUILD_DUELS` | ON | `duel_tN` binaries |
| `LR_BUILD_PROTOS` | ON | Prototype demos |
| `LR_ENABLE_REAL_RPM` | OFF | OpenProcess / RPM |
| `LR_ENABLE_REAL_SYSCALL` | OFF | Syscall path |
| `LR_ENABLE_REAL_KERNEL` | OFF | Driver / IOCTL |
| `LR_ENABLE_REAL_VMX` | OFF | VT-x helpers |
| `LR_ENABLE_REAL_DMA` | OFF | DMA / PCIe helpers |
| `LR_ENABLE_REAL_GPU` | OFF | D3D11 overlay path |
| `LR_ENABLE_REAL_UEFI` | OFF | UEFI firmware analysis backend |
| `LR_ENABLE_REAL_LINUX` | OFF | Linux real backend and host stubs |
| `LR_ENABLE_REAL_ALL` | OFF | Flip the real stack on |

All real and live-reader options default to `OFF` on every platform. Use
`LR_ENABLE_REAL_ALL=ON` for the complete supported real stack or enable only
the individual backends needed for an isolated lab exercise.

Runtime knobs: `LR_MODE=sim|real|hybrid`, `LR_VERBOSE=0|1`.

---

## Try it

```bash
# Strategy catalog (simulation)
./build/strategy_lab list
LR_MODE=sim ./build/strategy_lab run 01_external_rpm
./build/strategy_lab all --quiet

# Per-tier radar overlays (real path when CS2 is running; otherwise sim entities)
./build/radar_t0
./build/radar_t1

# Control panel demo (no game required)
./build/gui_demo

# Tests
ctest --test-dir build --output-on-failure
```

`INSERT` toggles the control panel in GUI demos; `END` / `ESC` quit. Settings land in `radar.ini` next to the binary.

---

## Docs

| Doc | What it is |
|-----|------------|
| [docs/INDEX.md](docs/INDEX.md) | Full index |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Threat model and design takeaways |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Code map and World model |
| [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md) | Build / test detail |
| [docs/THREAT-TIERS.md](docs/THREAT-TIERS.md) | T0–T4 delivery detail |
| [docs/STRATEGY-CATALOG.md](docs/STRATEGY-CATALOG.md) | Human index of strategy pairs |
| [docs/CURRICULUM.md](docs/CURRICULUM.md) | Suggested learning order |
| [code/STRUCTURE.md](code/STRUCTURE.md) | In-tree layout notes |

Live catalog truth is always `./build/strategy_lab list` (hundreds of registered pairs under `code/strategies/`).

---

## Safety / ethics

- Example vulnerable driver and DMA firmware are for **controlled lab use**. Do not load them on production machines.
- Real backends exercise OS APIs that commercial anti-cheat watches. Expect detection if you point them at a live game.
- This tree does **not** bypass HVCI/VBS, and it does **not** ship VMT hooks, injectors, or game-memory writers for gameplay advantage. Simulation plants scars on `sim::World`; real mode is read-oriented educational scaffolding.
- See [SECURITY.md](SECURITY.md).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: new lessons go under `strategies/<tier>/` with red + blue + `LESSON.md`, register them in `framework/registry.cpp`, and keep tests green.

---

## License

MIT — see [LICENSE](LICENSE). Educational use only; you own what you do with it.
