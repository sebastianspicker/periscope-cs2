# Radar

The game-facing radar product of [Periscope](../README.md): real adapters, live CS2 reading, a web radar service and an outbound relay. Not a cheat product and not a production anti-cheat.

The tiered red/blue teaching lab that shares this tree's history moved to
[`../anti-cheat/`](../anti-cheat/README.md). The dependency is strictly
one-way: radar consumes the lab's simulation world, types and strategy
libraries (`LR_ANTI_CHEAT_DIR`); the lab never imports radar code.

Two ways to run it:

| Mode | What it does | Needs |
|------|----------------|-------|
| Simulation demos | `radar_tN` replay deterministic scenarios on the anti-cheat lab's `sim::World` | C++20 + CMake + a `../anti-cheat` checkout. Any OS. |
| Real (Windows) | Attach to a live process (CS2 for T0/T1), read entities, draw a radar overlay | VS/MSVC or Clang-CL, optional WDK / VMX / DMA hardware for higher tiers |

Real Windows backends are optional via CMake; real backend features are
unsupported on Apple and stay off there.

`live_radar` prefers not to manufacture an information advantage: the default
`auto` policy starts at full intel and lets the degrade ladder fall back to
hud-echo (only what the in-game radar already shows) as blue pressure rises.
Posture and its limits: [../anti-cheat/docs/STEALTH-POSTURE.md](../anti-cheat/docs/STEALTH-POSTURE.md).

---

## Layout

```
.
├── README.md                 You are here
├── CMakeLists.txt            Build entry point (consumes ../anti-cheat libraries)
├── cmake/                    CMake helpers and target fragments
├── docs/                     Product docs (index, architecture, build, DMA policy)
├── src/
│   ├── domain/radar/         Snapshot, sampling, projection, scatter, mini-radar modules
│   └── application/radar/    Web radar service, tri_loader
├── adapters/real/            Optional OS/hardware backends (T0–T4)
├── apps/demos/               radar_tN, live_radar, probes, cs2_radar, tier_comparison
├── examples/drivers/         Example vulnerable driver (lab only)
├── examples/firmware/        Example PCIe DMA host sim
├── relay/                    Outbound-only radar relay (Python)
├── scripts/                  Signature and offset tooling
└── data/                     Offset snapshots and related data
```

---

## Build

### Portable default (any platform)

The radar tree needs the sibling anti-cheat checkout (default
`../anti-cheat`, override with `-DLR_ANTI_CHEAT_DIR=…`):

```bash
cmake -S radar -B radar/build/dev
cmake --build radar/build/dev -j
```

### Real backends (Windows)

```bash
cmake -S radar -B radar/build/dev -DLR_ENABLE_REAL_ALL=ON
cmake --build radar/build/dev -j
```

Useful CMake flags:

| Flag | Default | Meaning |
|------|:-------:|---------|
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

### Offsets and signatures

The live demos read entity data through offset/signature snapshots under
`data/cs2/` (`offsets.json`, `client_dll.json`, the generated
`cs2_offsets_snapshot.hpp`). The generator also installs the compiled copy at
`adapters/real/cs2/offsets_snapshot.hpp`; commit both sides together. Refresh
them with the tooling in `scripts/` (from `radar/`):

```bash
python scripts/update_cs2_signatures.py
```

---

## Try it

```bash
# Portable host tests (the required suite is included)
ctest --test-dir radar/build/dev --output-on-failure

# Per-tier deterministic simulation demos
./radar/build/dev/radar_t0
./radar/build/dev/radar_t1

# Windows GPU control panel demo (requires LR_ENABLE_REAL_GPU=ON)
./radar/build/dev/gui_demo

# Opt-in live CS2 radar (real RPM + syscall + GPU), auto posture by default
./radar/build/dev/live_radar
./radar/build/dev/live_radar --full          # pin full intel instead
```

The `radar-sim` verify lane also runs the anti-cheat lab's strategy catalog
from this build (`strategy_lab list` / `strategy_lab all --quiet`); the lab
itself is documented in [`../anti-cheat/README.md`](../anti-cheat/README.md).

`gui_demo` is created only when the real RPM, syscall, and GPU capabilities
are enabled. It uses simulated entities and does not require a game process.
`INSERT` toggles its control panel; `END` / `ESC` quit. Settings land in
`radar.ini` next to the binary.

`live_radar` defaults to the `auto` policy: it starts at full intel and lets the
degrade ladder drop to hud-echo when blue risk rises, never relaxing back past
hud-echo on its own. `--full` pins all-entity intel, `--strict-intel` pins
hud-echo and fails closed on an unreadable spotted flag. `I` cycles the policy
at runtime.
Posture, read shaping, and the degrade ladder: [../anti-cheat/docs/STEALTH-POSTURE.md](../anti-cheat/docs/STEALTH-POSTURE.md).

The web radar service and the outbound relay (`relay/`) let a game PC with no
inbound ports share a radar session with remote viewers; see
[relay/README.md](relay/README.md).

---

## Docs

| Doc | What it is |
|-----|------------|
| [docs/INDEX.md](docs/INDEX.md) | Full index |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Code map and World model |
| [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md) | Build / test detail |
| [docs/DMA-SCATTER-POLICY.md](docs/DMA-SCATTER-POLICY.md) | DMA scatter-read policy |
| [../anti-cheat/docs/OVERVIEW.md](../anti-cheat/docs/OVERVIEW.md) | Threat model and design takeaways |
| [../anti-cheat/docs/THREAT-TIERS.md](../anti-cheat/docs/THREAT-TIERS.md) | T0–T4 delivery detail |
| [../anti-cheat/docs/STRATEGY-CATALOG.md](../anti-cheat/docs/STRATEGY-CATALOG.md) | Human index of strategy pairs |
| [../anti-cheat/docs/CURRICULUM.md](../anti-cheat/docs/CURRICULUM.md) | Suggested learning order |
| [../anti-cheat/docs/STEALTH-POSTURE.md](../anti-cheat/docs/STEALTH-POSTURE.md) | Intel policy, read shaping, degrade ladder, offset pinning |

---

## Safety / ethics

- Example vulnerable driver and DMA firmware are for controlled lab use. Do not load them on production machines.
- Real backends exercise OS APIs that commercial anti-cheat watches. Expect detection if you point them at a live game.
- This tree does not bypass HVCI/VBS, and it does not ship VMT hooks, injectors, or game-memory writers for gameplay advantage. The simulation demos plant scars on the lab's `sim::World`; real mode is read-oriented educational scaffolding.
- See [SECURITY.md](SECURITY.md).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

MIT - see [LICENSE](LICENSE). Educational use only; you own what you do with it.
