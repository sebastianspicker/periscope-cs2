# Contributing

Educational anti-cheat lab (anti-cheat track of the Periscope monorepo). Useful contributions: clearer lessons, better blue sensors, or docs that match the tree.

## Monorepo

- Work that only touches this track: PR under `anti-cheat/` (this tree).
- Cross-cutting monorepo changes (shared CI, root layout, several tracks): follow parent-level contribution notes when they exist; otherwise say so in the PR description.
- The repository is local-only by standing decision: do not push or publish it.

## Ground rules

- Keep each lesson a genuine red/blue exercise with multi-step reds and multi-reason blues. One-bool "detected = true" lessons are not the quality bar.
- This tree's sources stay simulation-based: no `real/` (or `adapters/real/`) includes under `src/` or `scenarios/` - the architecture checker enforces it - and there are no real CMake flags here. Real-process capability lives in [`../radar/`](../radar/).
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
4. Add `red_example.cpp`, `blue_example.cpp` and `pair.cpp` to
   `cmake/ScenarioSources.cmake`.
5. Rebuild and run:
   ```bash
   ./build/dev/strategy_lab run <id>
   ```
6. If the pair introduces a new surface class, update the CMake wiring beyond the manifest.

## Verify

From the repository root:

```bash
python3 scripts/verify.py anti-cheat-sim
```

Or directly in this tree:

```bash
cmake -S . -B build/dev
cmake --build build/dev -j
ctest --test-dir build/dev --output-on-failure
./build/dev/strategy_lab all --quiet
```

## Doc updates

- Live catalog truth: `./build/dev/strategy_lab list`.
- Curriculum and the lesson index live under `docs/` (`docs/INDEX.md`).

## Pull requests

- One concern per PR when you can.
- Say what you ran (build flags).
- Update `docs/` only when behavior or layout actually changed.

## License

MIT - see [LICENSE](LICENSE).
