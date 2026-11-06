# Contributing

Game-facing radar product (radar track of the Periscope monorepo): real adapters, live CS2 reading, the web radar service and the outbound relay. Useful contributions: cleaner adapter seams, better radar domain modules, or docs that match the tree. The tiered red/blue teaching lab lives in [`../anti-cheat/`](../anti-cheat/README.md); lab and strategy-pair work is covered by [`../anti-cheat/CONTRIBUTING.md`](../anti-cheat/CONTRIBUTING.md).

## Monorepo

- Work that only touches this track: PR under `radar/` (this tree).
- Cross-cutting monorepo changes (shared CI, root layout, several tracks): follow parent-level contribution notes when they exist; otherwise say so in the PR description.

## Ground rules

- Keep it educational. Do not ship ready-to-run cheat tooling aimed at live competitive play.
- Simulation is the default path. Real backends stay behind CMake flags and should degrade cleanly when hardware or the game is missing.
- Radar consumes the anti-cheat lab one-way (`LR_ANTI_CHEAT_DIR`); do not add code paths that the lab would need to import from radar.
- Match existing style in the folder you touch. Headers and sources live together under `src/` and `adapters/real/`.

## Verify

From the repository root:

```bash
python3 scripts/verify.py radar-sim
```

Or directly in this tree:

```bash
cmake -S . -B build/dev
cmake --build build/dev -j
ctest --test-dir build/dev --output-on-failure
```

Real backends additionally need `-DLR_ENABLE_REAL_*` flags and a Windows host;
see [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md).

## Doc updates

- Product docs live under `docs/` (`docs/INDEX.md` is the index).
- Strategy catalog and curriculum truth moved with the lab: see [`../anti-cheat/CONTRIBUTING.md`](../anti-cheat/CONTRIBUTING.md).

## Pull requests

- One concern per PR when you can.
- Say what you ran (build flags).
- Update `docs/` only when behavior or layout actually changed.
