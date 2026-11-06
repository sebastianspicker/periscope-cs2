# Anti-Cheat Lab

Tiered red/blue teaching lab for **external "legit" radar** — tools that read game memory and draw a map without aiming for you — and the detection techniques that push back. Every lesson runs over a simulated, CS2-shaped world (`sim::World`); the lab has no real backends.

This is the **anti-cheat** track of [Periscope](../README.md). Sister tracks: [`../radar/`](../radar/) (the game-facing radar product, which consumes this tree's libraries), [`../vision/`](../vision/) and [`../lens/`](../lens/). Not a cheat product and not a production anti-cheat.

One way to run it:

| Mode | What it does | Needs |
|------|----------------|-------|
| **Simulation** | Red plants scars on a deterministic `sim::World`; blue reasons over them | C++20 + CMake. Any OS. |

The default (and only) configuration is simulation. Real Windows backends are
not part of this tree; they live in [`../radar/`](../radar/) behind that tree's
own CMake flags.

---

## Why it exists

Commercial "looks legit" radar packs escalate delivery as blue closes lower scars:

```
T0  OpenProcess + ReadProcessMemory
T1  Same handle, indirect / direct syscalls (hooks are theater)
T2  Kernel / BYOVD / memrw device (no game handle)
T3  Personal hypervisor + thin bridge
T4  Off-box DMA / capture residuals
```

Each tier has a red team library, a blue team library, runnable demos, and strategy pair lessons (apply → detect) under `scenarios/strategies/{t0..t4,crosscutting}/`.

---

## Layout

```
.
├── README.md                 You are here
├── CMakeLists.txt            Build entry point (project ac_lab)
├── cmake/                    CMake helpers and target fragments
├── docs/                     Curriculum and reference (index: docs/INDEX.md)
├── src/
│   ├── domain/               Core values, telemetry, server-side policy
│   ├── simulation/           Deterministic world and scenario engines
│   ├── application/          Detection, analysis, strategies, simulation pipeline
│   └── lab_components/       Lab infrastructure and tier components
├── scenarios/strategies/     Red/blue pair lessons + strategy_lab
├── apps/demos/               duel_tN, proto_tN_*, labs, fps_demo
├── scripts/                  Lab verification scripts
└── tests/                    stack_posture host test
```

---

## Build

### Simulation only (any platform)

From the repository root:

```bash
cmake -S anti-cheat -B anti-cheat/build/dev
cmake --build anti-cheat/build/dev -j
```

The tree configures and builds with no real flags set — there are none to set.

Useful CMake flags:

| Flag | Default | Meaning |
|------|:-------:|---------|
| `LR_BUILD_STRATEGY_LAB` | ON | `strategy_lab` CLI |
| `LR_BUILD_DUELS` | ON | `duel_tN` binaries |
| `LR_BUILD_PROTOS` | ON | Prototype demos |

See `cmake/Options.cmake` for the `LAB_ALLOW_*` flags that enable additional
educational simulation surfaces.

---

## Try it

```bash
# Strategy catalog (simulation)
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run 01_external_rpm
./anti-cheat/build/dev/strategy_lab all --quiet

# Portable host tests (the required suite is included)
ctest --test-dir anti-cheat/build/dev --output-on-failure

# Per-tier deterministic simulation demos
./anti-cheat/build/dev/fps_demo
./anti-cheat/build/dev/duel_t0
```

Lab applications:

| App | What it is |
|-----|------------|
| `strategy_lab` | Strategy catalog runner over `scenarios/strategies/` |
| `duel_t0` … `duel_t4` | Red-vs-blue duel demos |
| `proto_t0_red` … `proto_t4_blue` | Per-tier prototype demos |
| `fps_demo` | FPS game state demo |
| `evasion_lab`, `features_lab`, `ops_lab`, `structural_lab` | Simulation surface demos |

---

## One-way dependency

Radar consumes the lab: radar's real adapters include this tree's `ac/…`
types, `ac_sim/…` headers and team/component libraries, and radar's CMake
loads this tree's library targets through `LR_ANTI_CHEAT_DIR`. The lab never
imports radar code. The single exception is the test-only seam in
`tests/stack_posture_test.cpp`, which quote-includes the header-only
`real/cs2/posture.hpp` from the radar tree through the `LR_RADAR_DIR` CMake
path.

---

## Docs

| Doc | What it is |
|-----|------------|
| [docs/INDEX.md](docs/INDEX.md) | Curriculum index |
| [docs/CURRICULUM.md](docs/CURRICULUM.md) | Suggested learning order |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Threat model and design takeaways |
| [docs/THREAT-TIERS.md](docs/THREAT-TIERS.md) | T0–T4 delivery detail |
| [docs/STRATEGY-CATALOG.md](docs/STRATEGY-CATALOG.md) | Human index of strategy pairs |
| [docs/STEALTH-POSTURE.md](docs/STEALTH-POSTURE.md) | Intel policy, read shaping, degrade ladder, offset pinning |

Live catalog truth is always `anti-cheat/build/dev/strategy_lab list` (hundreds of registered pairs under `scenarios/strategies/`).

---

## Safety / ethics

- This tree is simulation-only: red plants scars on `sim::World`; nothing here opens a process, reads game memory, or touches hardware.
- The example vulnerable driver and DMA firmware are **not** in this tree; they are lab-only material of the radar tree — see [`../radar/README.md`](../radar/README.md) and [`../radar/SECURITY.md`](../radar/SECURITY.md).
- See [SECURITY.md](SECURITY.md).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: new lessons go under `scenarios/strategies/<tier>/` with red + blue + `LESSON.md`, register them in `scenarios/strategies/framework/registry.cpp`.

---

## License

MIT — see [LICENSE](LICENSE). Educational use only; you own what you do with it.
