# Shared packages

Reusable lab infrastructure under `code/lib/`.

## Layout

| Package | Path | Role |
|---------|------|------|
| ac | `lib/ac/` | Core types: Status, Tier, EntitySnapshot, telemetry, risk |
| sim | `lib/sim/` | `sim::World` arena — processes, handles, drivers, trust flags, inputs |
| ac_sim | `lib/ac_sim/` | Temporal engine, behavioral filter, forensic / health helpers |
| server | `lib/server/` | Interest management, info-advantage, ban correlator |
| depth | `lib/depth/` | Multi-sample handles, leakage, multi-invariant, trust, seller fusion |
| fps | `lib/fps/` | Bomb plant/defuse map, scenario, lab bridge |
| lab | `lib/lab/` | Fixture process, LabMemoryBackend, pattern scanners |
| cs2 | `lib/cs2/` | CS2-shaped helpers, signatures, diagnostics |
| blue | `lib/blue/` | Shared multi-view blue coordinator |
| strategies (support) | `lib/strategies/` | Pair utilities, multi-reason helpers, scar sensors |
| real | `lib/real/` | Optional OS/hardware backends (see ARCHITECTURE.md) |

## sim::World

Single battlefield both red and blue mutate. Team libraries and strategies plant scars here. Public include is `sim/world.hpp` (types and fields are split across `world_types.hpp`, `world_api.hpp`, and `.inc` fragments).

## Lab vs tests

- Lab library code: `lib/lab/`
- Tests: `code/tests/` (team API, depth, fps, tier full_tests, real backend smoke)

## CS2 helpers

`lib/cs2/` supports `demos/cs2_radar/tN/` and live radar demos. Signature tooling lives under `code/scripts/`.

## Team packages

| Package | Role |
|---------|------|
| `teams/t0_red/` … `teams/t4_red/` | Red delivery surfaces |
| `teams/t0_blue/` … `teams/t4_blue/` | Blue sensors and agents |
