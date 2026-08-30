# Shared packages

Reusable lab infrastructure under `src/`.

## Layout

| Package | Path | Role |
|---------|------|------|
| ac | `src/domain/ac/` | Core types: Status, Tier, EntitySnapshot, telemetry, risk |
| sim | `src/simulation/sim/` | `sim::World` arena — processes, handles, drivers, trust flags, inputs |
| ac_sim | `src/simulation/ac_sim/` | Temporal engine, behavioral filter, forensic / health helpers |
| server | `src/domain/server/` | Interest management, info-advantage, ban correlator |
| depth | `src/application/analysis/depth/` | Multi-sample handles, leakage, multi-invariant, trust, seller fusion |
| fps | `src/simulation/fps/` | Bomb plant/defuse map, scenario, lab bridge |
| lab | `src/lab_components/lab/` | Fixture process, LabMemoryBackend, pattern scanners |
| cs2 | `src/simulation/cs2/` | CS2-shaped helpers, signatures, diagnostics |
| blue | `src/application/detection/blue/` | Shared multi-view blue coordinator |
| strategies (support) | `src/application/strategies/` | Pair utilities, multi-reason helpers, scar sensors |
| real | `adapters/real/` | Optional OS/hardware backends (see ARCHITECTURE.md) |

## sim::World

Single battlefield both red and blue mutate. Team libraries and strategies plant scars here. Public include is `sim/world.hpp` (types and fields are split across `world_types.hpp`, `world_api.hpp`, and `.inc` fragments).

## Lab vs tests

- Lab library code: `src/lab_components/lab/`
- Tests: `tests/` (team API, depth, fps, tier full_tests, real backend smoke)

## CS2 helpers

`src/simulation/cs2/` supports `apps/demos/cs2_radar/tN/` and live radar demos. Signature tooling lives under `scripts/`.

## Team packages

| Package | Role |
|---------|------|
| `src/lab_components/teams/t0_red/` … `src/lab_components/teams/t4_red/` | Red delivery surfaces |
| `src/lab_components/teams/t0_blue/` … `src/lab_components/teams/t4_blue/` | Blue sensors and agents |
