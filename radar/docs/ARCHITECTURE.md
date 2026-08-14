# Architecture

## Monorepo layout

```
code/
├── CMakeLists.txt
├── cmake/                         CMake helpers / generated headers
├── lib/                           Libraries (headers + sources together)
│   ├── ac/                        Core types (Tier, EntitySnapshot, Vec3, Status)
│   ├── sim/                       sim::World — scar arena
│   ├── ac_sim/                    Temporal engine, behavioral filter, helpers
│   ├── server/                    InterestManager, InfoAdvantageScorer, BanCorrelator
│   ├── depth/                     Multi-invariant analysis, trust / seller fusion
│   ├── fps/                       FPS game state machine, scenario runner
│   ├── lab/                       Fixture process, pattern scanners
│   ├── cs2/                       CS2 models, diagnostics, signatures
│   ├── blue/                      Blue team coordinator
│   ├── strategies/                Shared pair support (scorers, sensors)
│   └── real/                      Platform backends (optional at configure time)
│       ├── win/                   NT API table, syscalls, PE/ETW helpers
│       ├── cs2/                   Process attach, HijackReader, radar stack
│       ├── kernel/                Driver load, IOCTL, BYOVD scaffolding
│       ├── dma/                   PCIe / FPGA / Thunderbolt
│       ├── vmx/                   Intel VT-x
│       ├── smm/                   SMM / ACPI / TPM-shaped surfaces
│       ├── uefi/                  UEFI firmware lab helpers
│       ├── gpu/                   D3D11 overlay, GUI, render pipeline
│       ├── net/                   Sockets, HTTP, C2-shaped client, pipes
│       ├── mode/                  Runtime mode (real / sim / hybrid)
│       └── linux/                 Linux memory / process helpers
├── teams/                         t0_red … t4_blue
├── strategies/                    Pair lessons + strategy_lab
│   ├── framework/                 CLI, registry, runner
│   ├── t0/ … t4/                  Tier pairs
│   └── crosscutting/              Cross-cutting pairs
├── demos/                         Runnable executables
├── drivers/                       Kernel driver examples (WDK lab)
├── firmware/                      FPGA DMA host-sim examples
├── tests/                         CTest sources
├── scripts/                       Signature / offset tooling
└── data/                          Offset snapshots and related data
```

## Libraries

Each library target keeps headers and sources in one directory. There is no separate top-level `include/` tree.

| Target | Path | Purpose |
|--------|------|---------|
| ac_common | `lib/ac/` | Core types, telemetry, risk |
| ac_sim | `lib/sim/` + `lib/ac_sim/` | World arena + temporal/behavioral helpers |
| ac_server | `lib/server/` | Interest, info-advantage, bans |
| ac_depth | `lib/depth/` | Multi-sample / trust / fusion scorers |
| ac_lab | `lib/lab/` | Fixture process, lab memory, scanners |
| ac_fps | `lib/fps/` | FPS map and scenario |
| ac_cs2 | `lib/cs2/` | CS2-shaped helpers and signatures |
| ac_t0_red … ac_t4_blue | `teams/tN_*` | Tier team libraries |
| ac_strategies | `strategies/` | Catalog registry + all pairs |
| ac_real_platform | `lib/real/…` | Optional real backends |

Exact CMake target names can vary slightly; see `code/CMakeLists.txt`.

## sim::World: single source of truth

Strategies and team libraries interact through `sim::World` fields. The World represents game-shaped state plus detectable scars:

- Processes, handles, modules (RPM, inject, hollowing, hooks)
- Drivers, devices (BYOVD, IOCTL, HV bridge)
- Trust flags (VBS, HVCI, personal HV, IOMMU, DMA, attestation)
- Input and aim samples (humanization, silent aim, provenance)
- Accounts, network, overlays (graph, C2, ESP)
- Operations residuals (VPN, overwatch, clipboard)

## Multi-step pattern

```
Red::apply(World&)
  Plants scars A and B on the World
  Achieved only if multi-field narrative conditions hold

Blue::detect(World&)
  multi_reason / score / fog
  Returns detected or mitigated with evidence
```

## Demo and test dependencies

```
strategy_lab  -> ac_strategies -> team libs + sim/depth/server
duel_tN / proto_tN_* -> team libs
radar_t0 … t4 / live_radar -> ac_real_platform (when enabled) + sim
tests/* -> shipped apply/detect or team API entry points
fps_demo -> ac_fps + ac_server
```

## Data flow examples

### T0 external RPM

```
reader.OpenProcess(game, VM_READ)
  -> NtReadVirtualMemory(handle, entity_blob)
  -> project world to radar coordinates
  -> render overlay
Blue: handle graph analysis on World.processes
```

### T2 BYOVD (simulation)

```
clear driver mitigation flags
  -> load vulnerable driver into World.drivers
  -> IOCTL read via World.devices
  -> no game handle in World.processes
Blue: driver blocklist, device watch, callback integrity
```

### T4 DMA (simulation)

```
dma_device + iommu_off -> dma_read
  -> no local process involved
Blue: IOMMU state signal + leakage fog + InfoAdvantage scoring
```
