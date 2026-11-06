# Architecture

Radar is the game-facing product of the Periscope monorepo: optional
platform backends that read the live game, the applications built on them
(web radar service, tri_loader) and an outbound relay. The tiered red/blue
teaching lab that shares this tree's history — the `sim::World`, the evasion
components and the strategy catalog — lives in the sibling
[`anti-cheat/`](../../anti-cheat/README.md) tree; its docs are indexed at
[`anti-cheat/docs/INDEX.md`](../../anti-cheat/docs/INDEX.md).

## Tree layout

```
radar/
├── CMakeLists.txt                 Build entry point (consumes ../anti-cheat libraries)
├── cmake/                         Options (real flags), RealTargets, AppTargets, TestTargets
├── src/
│   ├── domain/radar/              Snapshot, sampling, projection, scatter, LOS, mini-radar modules
│   └── application/radar/         Web radar service, tri_loader
├── adapters/real/                 Optional OS/hardware backends (T0–T4)
├── apps/demos/                    radar_tN, live_radar, probes, cs2_radar, tier_comparison
├── examples/drivers/              Kernel driver examples (WDK lab)
├── examples/firmware/             FPGA DMA host-sim examples
├── relay/                         Outbound-only radar relay (Python)
├── scripts/                       Signature / offset tooling
├── tests/                         radar_* host tests
└── data/                          Offset snapshots and related data
```

## Libraries

Each library target keeps headers and sources in one directory. There is no separate top-level `include/` tree.

| Target | Path | Purpose |
|--------|------|---------|
| radar_snapshot | `src/domain/radar/` | Snapshot, projection, mini-radar, offset overrides, scatter batching, LOS BVH |
| radar_webradar | `src/application/radar/` | Transport-independent web radar service core |
| radar_collision | `src/application/radar/` | Map-collision `.tri` file loader |
| ac_real_core, ac_real_rpm, ac_real_syscall, … | `adapters/real/…` | Narrow opt-in capability backends |
| ac_real_pipeline | `adapters/real/application/` | Opt-in live pipeline composition |

The `radar_*` libraries are pure portable logic: no real adapter and no
simulation layer underneath, which is what lets the host test suite run on
every platform.

Exact CMake target names can vary slightly; see `CMakeLists.txt` and `cmake/`.

## Dependency direction: radar consumes the anti-cheat lab, one-way

The lab subtree is configured through a single seam, `LR_ANTI_CHEAT_DIR`
(default `../anti-cheat`, override with `-DLR_ANTI_CHEAT_DIR=…`; configure
fails with a clear error when it does not point at an anti-cheat checkout).
Through that seam radar includes the lab's library CMake and its `src/` include
root, so the existing `"ac/…"`, `"sim/…"`, `"ac_sim/…"` quote-includes keep
working verbatim — the real pipeline (`adapters/real/application/`) composes
the lab's evasion components, and the sim fallback of the `cs2_radar` family
links the lab's pipeline library.

The direction is strictly one-way:

- The lab never includes radar CMake or imports radar code (enforced by the
  architecture checker); it builds with no real flags set.
- The only seam back across the boundary is a test: the lab's
  `stack_posture_test` includes radar's `adapters/real/cs2/posture.hpp`
  (`real/cs2/posture.hpp` through radar's adapters include root) because the
  intel policy and read-shaping posture it verifies are product logic.

## Demo and test dependencies

```
radar_t0 … radar_t4  -> ac_common + ac_sim (the lab's sim::World)
cs2_radar_t0         -> ac_real_pipeline (real) or the lab's ac_pipeline + team libs (sim fallback)
cs2_radar_t1 … t4    -> team libs + ac_cs2 + ac_sim
live_radar           -> selected ac_real_* capability targets + sim (when enabled)
radar_*_test         -> radar_snapshot / radar_webradar / radar_collision
```

`strategy_lab`, the sim-only demos (`duel_tN`, `proto_tN_*`, `fps_demo`, the
labs) and the full lab target graph are registered by the lab's own CMake and
documented in [`anti-cheat/README.md`](../../anti-cheat/README.md); in a radar
configure they are built into radar's build directory so the `radar-sim`
verify lane can run the catalog from here.

## Live read path

```
attach / find game process (adapters/real/process.*, cs2/process_finder.*)
  -> read entities (adapters/real/cs2/entities.*, memory.*)
  -> project world to radar coordinates (src/domain/radar/projection.*)
  -> render overlay, or publish to the web radar service (src/application/radar/webradar_service.*)
  -> remote viewers receive it through the outbound relay (relay/)
```

Backends are opt-in per capability: every `LR_ENABLE_REAL_*` backend lives in
its own `adapters/real/` area and nothing real is linked unless its capability
targets exist. See [BUILD-AND-TEST.md](BUILD-AND-TEST.md) for the options.
