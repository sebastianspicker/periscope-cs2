# `code/` — build tree

CMake entry point for the lab. Root overview lives in [`../README.md`](../README.md).

## Quick build

```bash
cmake -S . -B build -DLR_BUILD_TESTS=ON -DLR_BUILD_STRATEGY_LAB=ON
cmake --build build -j

# Windows full real stack
cmake -S . -B build -DLR_ENABLE_REAL_ALL=ON
cmake --build build -j
```

```bash
./build/strategy_lab list
LR_MODE=sim ./build/strategy_lab run 01_external_rpm
ctest --test-dir build --output-on-failure
```

## Layout

| Path | Role |
|------|------|
| `lib/ac/` | Core types (`Tier`, `EntitySnapshot`, telemetry) |
| `lib/sim/` | `sim::World` — the scar arena both teams write |
| `lib/ac_sim/` | Temporal engine, behavioral filter, lab helpers |
| `lib/server/` | Interest management, info-advantage, ban correlator |
| `lib/depth/` | Multi-signal / trust fusion scorers |
| `lib/fps/` | FPS scenario machine |
| `lib/lab/` | Fixture process, AOB scanner, lab memory |
| `lib/cs2/` | CS2-shaped models, signatures, diagnostics |
| `lib/blue/` | Blue coordinator |
| `lib/real/` | Platform backends (win, cs2, kernel, vmx, dma, gpu, net, …) |
| `teams/` | `t0_red` … `t4_blue` team libraries |
| `strategies/` | Pair lessons + `strategy_lab` framework |
| `demos/` | Radar, duels, probes, GUI |
| `drivers/`, `firmware/` | Lab-only BYOVD / DMA examples |
| `tests/` | CTest sources |
| `docs/` | Real-AC notes that sit next to the code |
| `cmake/` | Helper scripts / generated-header templates |

## Docs

| Doc | Notes |
|-----|--------|
| [`STRUCTURE.md`](STRUCTURE.md) | Longer layout notes |
| [`PROTOTYPES.md`](PROTOTYPES.md) | Per-tier proto / CS2 radar binaries |
| [`../docs/`](../docs/) | Curriculum and architecture |
| [`docs/`](docs/) | Real AC learnings, pair→real map |

## Rules of thumb

1. Headers and sources live together under `lib/` (no split `include/` / `src/`).
2. OS/hardware work stays under `lib/real/`.
3. New technique lessons: `strategies/<tier>/<name>/` with `red_example`, `blue_example`, `pair.cpp`, optional `LESSON.md`, then register in `strategies/framework/registry.cpp`.
4. Prefer multi-step red and multi-reason blue over single-bool flags.
