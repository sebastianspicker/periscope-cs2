# Curriculum

A practical path through the lab. Paths are relative to the repo root unless noted.

## 0. Orientation

1. [OVERVIEW.md](OVERVIEW.md) - threat model and design takeaways
2. [ARCHITECTURE.md](../../docs/ARCHITECTURE.md) - monorepo map and where `sim::World` lives
3. Lab [README.md](../README.md) - build flags and first binaries
4. [BUILD-AND-TEST.md](../../radar/docs/BUILD-AND-TEST.md) - the radar tree's cmake, for the real path

## 1. Shared world

1. Read `src/simulation/sim/world.hpp` (and the split headers it pulls in) - scar inventory
2. [SHARED.md](SHARED.md) - what each `src/` package does
3. Run `./anti-cheat/build/dev/fps_demo all`

## 2. Delivery ladder (T0 → T4)

For each tier:

1. `docs/tiers/tN/LESSON.md` and `README.md`
2. Skim `src/lab_components/teams/tN_red/` and `src/lab_components/teams/tN_blue/`
3. Run `./anti-cheat/build/dev/duel_tN` and the matching `proto_tN_*` binaries
4. Optional (radar tree): `radar/build/dev/cs2_radar_tN` or `radar/build/dev/radar_tN` - see [`../../radar/README.md`](../../radar/README.md)

Order: T0 → T1 → T2 → T3 → T4. Detail: [THREAT-TIERS.md](THREAT-TIERS.md).

## 3. Strategy catalog

1. `./anti-cheat/build/dev/strategy_lab list`
2. Run a pair: `./anti-cheat/build/dev/strategy_lab run 01_external_rpm`
3. [STRATEGY-CATALOG.md](STRATEGY-CATALOG.md) for human one-liners
4. [TIERED-COUNTERS.md](TIERED-COUNTERS.md) for the red/blue matrix

## 4. Crosscutting families

Still under `scenarios/strategies/`; family is catalog metadata, not a separate delivery tier.

1. [CROSSCUTTING.md](CROSSCUTTING.md)
2. `./anti-cheat/build/dev/evasion_lab`, `features_lab`, `ops_lab`, `structural_lab`

## 5. Real path (Windows, optional)

1. Configure with the real flags you need (`live_radar` requires real RPM, syscall, and GPU capabilities)
2. Build and run `live_radar` against CS2 on a supported Windows lab host
3. Use [ARCHITECTURE.md](../../radar/docs/ARCHITECTURE.md) (radar tree) and [TOOLS.md](TOOLS.md) for the supported composition paths
4. Stay on lab machines for kernel / VMX / DMA experiments ([SECURITY.md](../SECURITY.md))

## 6. Depth

1. `src/application/analysis/depth/` scorers and [SHARED.md](SHARED.md)
2. Server residual: interest management + info-advantage (`src/domain/server/`)
3. [RESIDUAL-RESEARCH.md](RESIDUAL-RESEARCH.md)
