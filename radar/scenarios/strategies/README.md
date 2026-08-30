# Strategy pairs

Red/blue lessons for delivery tiers and crosscutting families.

```text
strategies/
├── framework/           # strategy_lab CLI + registry
├── t0/ … t4/            # tier-specific pairs
└── crosscutting/        # features, ops, structural, shared evasion
    <name>/
    ├── red_example.hpp / .cpp
    ├── blue_example.hpp / .cpp
    ├── pair.cpp           # StrategyEntry factory
    └── LESSON.md          # optional
```

- Catalog registration: `framework/registry.cpp`
- CMake: explicit source inventory in `cmake/ScenarioTargets.cmake`
- Runner: `./radar/build/dev/strategy_lab` from the repository root
- Path index: [CATALOG.md](CATALOG.md)
- One-liners: [`../../docs/STRATEGY-CATALOG.md`](../../docs/STRATEGY-CATALOG.md)

Family (Delivery / Feature / Evasion / Detection / Structural) is metadata on each entry, not a hard directory split. Crosscutting pairs still live under `crosscutting/` for convenience.
