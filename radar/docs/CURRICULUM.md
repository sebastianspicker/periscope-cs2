# Curriculum

A practical path through the lab. Paths are relative to the repo root unless noted.

## 0. Orientation

1. [OVERVIEW.md](OVERVIEW.md) — threat model and design takeaways
2. [ARCHITECTURE.md](ARCHITECTURE.md) — layout and World model
3. [BUILD-AND-TEST.md](BUILD-AND-TEST.md) — cmake / ctest
4. Root [README.md](../README.md) — build flags and first binaries

## 1. Shared world

1. Read `code/lib/sim/world.hpp` (and the split headers it pulls in) — scar inventory
2. [SHARED.md](SHARED.md) — what each `lib/` package does
3. Run `./build/fps_demo all`

## 2. Delivery ladder (T0 → T4)

For each tier:

1. `docs/tiers/tN/LESSON.md` and `README.md`
2. Skim `code/teams/tN_red/` and `code/teams/tN_blue/`
3. Run `./build/duel_tN` and the matching `proto_tN_*` binaries
4. Optional: `./build/cs2_radar_tN` or `./build/radar_tN`

Order: T0 → T1 → T2 → T3 → T4. Detail: [THREAT-TIERS.md](THREAT-TIERS.md).

## 3. Strategy catalog

1. `./build/strategy_lab list`
2. Run a pair: `./build/strategy_lab run 01_external_rpm`
3. [STRATEGY-CATALOG.md](STRATEGY-CATALOG.md) for human one-liners
4. [TIERED-COUNTERS.md](TIERED-COUNTERS.md) for the red/blue matrix

## 4. Crosscutting families

Still under `code/strategies/`; family is catalog metadata, not a separate delivery tier.

1. [CROSSCUTTING.md](CROSSCUTTING.md)
2. `./build/evasion_lab`, `features_lab`, `ops_lab`, `structural_lab`

## 5. Real path (Windows, optional)

1. Configure with the real flags you need (`LR_ENABLE_REAL_RPM` is the usual first step)
2. `./build/live_radar` or `./build/radar_t0` against CS2 if present
3. `code/docs/REAL-AC-LEARNINGS.md` and `PAIR-TO-REAL-MAP.md`
4. Stay on lab machines for kernel / VMX / DMA experiments ([SECURITY.md](../SECURITY.md))

## 6. Depth

1. `code/lib/depth/` scorers and [SHARED.md](SHARED.md)
2. Server residual: interest management + info-advantage (`code/lib/server/`)
3. [RESIDUAL-RESEARCH.md](RESIDUAL-RESEARCH.md)
