# Contributing

Educational anti-cheat research lab (radar track of the Periscope monorepo). Useful contributions: clearer lessons, better blue sensors, tighter tests, or docs that match the tree.

## Monorepo

- Work that only touches this track: PR under `radar/` (this tree).
- Cross-cutting monorepo changes (shared CI, root layout, both tracks): follow parent-level contribution notes when they exist; otherwise say so in the PR description.

## Ground rules

- Keep it educational. Do not ship ready-to-run cheat tooling aimed at live competitive play.
- Prefer multi-step red and multi-reason blue. One-bool “detected = true” lessons are not the quality bar.
- Simulation is the default path. Real backends stay behind CMake flags and should degrade cleanly when hardware or the game is missing.
- Match existing style in the folder you touch. Headers and sources live together under `src/` and `src/lab_components/teams/`.

## New strategy pair

1. Pick a tier folder (or `crosscutting/`):
   ```
   scenarios/strategies/<tier>/<short_name>/
     red_example.hpp / red_example.cpp
     blue_example.hpp / blue_example.cpp
     pair.cpp
     LESSON.md          # what the scar is, what blue looks for
   ```
2. Export `entry_NN_short_name()` from `pair.cpp`.
3. Register it in `scenarios/strategies/framework/registry.cpp`.
4. Rebuild and run:
   ```bash
   ./build/strategy_lab run <id>
   ctest --test-dir build --output-on-failure
   ```
5. If the pair introduces a new surface class, update the CMake wiring and retained CTest smoke coverage when applicable.

## Doc updates

- Live catalog truth: `./build/strategy_lab list`.
- Curriculum and architecture live under `docs/`.

## Pull requests

- One concern per PR when you can.
- Say what you ran (build flags, which tests).
- Update `docs/` only when behavior or layout actually changed.
