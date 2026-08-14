# Project structure

How the `code/` tree is organized. For build flags and usage, see the [root README](../README.md).

```
code/
├── CMakeLists.txt
├── cmake/                    Helper scripts, generated headers
├── lib/
│   ├── ac/                   Core types, telemetry, risk
│   ├── sim/                  sim::World (scar arena)
│   ├── ac_sim/               Temporal / behavioral / forensic helpers
│   ├── server/               Interest, info-advantage, bans
│   ├── depth/                Multi-invariant / trust / seller fusion
│   ├── fps/                  FPS map + scenario
│   ├── lab/                  Fixture process, scanners, lab memory
│   ├── cs2/                  CS2 models, signatures, diagnostics
│   ├── blue/                 Blue coordinator
│   ├── strategies/           Shared strategy support (scorers, pair util)
│   └── real/                 Real OS / hardware backends
│       ├── win/              NT API table, syscalls, PE/ETW helpers
│       ├── cs2/              Process attach, entity read, radar stack
│       ├── kernel/           Driver load, IOCTL, BYOVD scaffolding
│       ├── dma/              PCIe / FPGA / Thunderbolt paths
│       ├── vmx/              VT-x lifecycle, EPT, VMCS
│       ├── smm/              SMM / ACPI / TPM-shaped surfaces
│       ├── uefi/             UEFI firmware lab helpers
│       ├── gpu/              D3D11 overlay, GUI, render pipeline
│       ├── net/              Sockets, HTTP, C2-shaped client, pipes
│       ├── mode/             sim / real / hybrid selection
│       └── linux/            Linux memory / process helpers
├── teams/                    t0_red … t4_blue
├── strategies/
│   ├── framework/            CLI, registry, runner
│   ├── t0/ … t4/             Tier delivery / evasion pairs
│   └── crosscutting/         Features, ops, structural pairs
├── demos/                    Executables (radar, duels, probes, GUI)
├── drivers/example_vulnerable/
├── firmware/example_pcie_dma/
├── tests/
├── scripts/                  Offset / signature tooling
├── data/cs2/                 Offset snapshots / signature data
└── docs/                     Code-adjacent research notes
```

## Conventions

1. **Co-located headers** — each library keeps `.hpp` and `.cpp` in the same folder.
2. **Teams vs libs** — tier attack/defense logic lives in `teams/`; shared infrastructure in `lib/`.
3. **Real stays under `real/`** — anything that touches OS/hardware APIs goes there; sim strategies never import it unless the demo opts into real backends.
4. **Strategies are lessons** — one folder per pair, registered in `framework/registry.cpp`. CMake GLOBs `strategies/**/*.cpp`.
5. **Include paths** — prefer `#include "ac/types.hpp"`, `#include "sim/world.hpp"`, etc. CMake sets the roots.

## Main binaries

| Binary | Source area |
|--------|-------------|
| `strategy_lab` | `strategies/framework/` |
| `radar_t0` … `radar_t4` | `demos/radar_tN.cpp` |
| `live_radar` | `demos/live_radar*.cpp` |
| `gui_demo` | `demos/gui_demo.cpp` |
| `tier_comparison` | `demos/tier_comparison.cpp` |
| `duel_tN` / `proto_tN_*` | `demos/` |
| `cs2_radar_tN` | `demos/cs2_radar/tN/` |
| `fps_demo` | `demos/fps_demo.cpp` |

## Tests

All CTest sources live under `tests/`. Configure with `-DLR_BUILD_TESTS=ON` and run:

```bash
ctest --test-dir build --output-on-failure
```
