# Architecture

## Monorepo layout

```
.
├── CMakeLists.txt                 Build entry point
├── cmake/                         CMake helpers / generated headers
├── src/
│   ├── domain/                    Core values, telemetry, server-side policy
│   ├── simulation/                Deterministic world and scenario engines
│   ├── application/               Detection, analysis, strategies, simulation pipeline
│   └── lab_components/             Lab infrastructure and tier components
├── adapters/real/                 Optional platform backends
├── scenarios/strategies/           Pair lessons + strategy_lab
├── apps/demos/                    Runnable executables
├── examples/drivers/              Kernel driver examples (WDK lab)
├── examples/firmware/             FPGA DMA host-sim examples
├── scripts/                       Signature / offset tooling
└── data/                          Offset snapshots and related data
```

## Libraries

Each library target keeps headers and sources in one directory. There is no separate top-level `include/` tree.

| Target | Path | Purpose |
|--------|------|---------|
| ac_common | `src/domain/ac/` | Core types, telemetry, risk |
| ac_sim | `src/simulation/sim/` + `src/simulation/ac_sim/` | World arena + temporal/behavioral helpers |
| ac_server | `src/domain/server/` | Interest, info-advantage, bans |
| ac_strategy_core | `src/application/strategies/` | Strategy contracts and shared policy |
| ac_blue | `src/application/detection/blue/` | Simulation-backed detection policy |
| ac_depth | `src/application/analysis/depth/` | Multi-sample / trust / fusion scorers |
| ac_lab | `src/lab_components/lab/` | Fixture process, lab memory, scanners |
| ac_fps | `src/simulation/fps/` | FPS map and scenario |
| ac_cs2 | `src/simulation/cs2/` | CS2-shaped helpers and signatures |
| ac_t0_red … ac_t4_blue | `src/lab_components/teams/tN_*` | Tier team libraries |
| ac_strategies | `scenarios/strategies/` | Catalog registry + all pairs |
| ac_real_core, ac_real_rpm, ac_real_syscall, … | `adapters/real/…` | Narrow opt-in capability backends |
| ac_real_pipeline | `adapters/real/application/` | Opt-in live pipeline composition |

Exact CMake target names can vary slightly; see `CMakeLists.txt`.

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
radar_t0 … t4 -> sim
live_radar -> selected ac_real_* capability targets + sim (when enabled)
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
