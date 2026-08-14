# Periscope

Umbrella monorepo for two CS2-related research tracks with different threat models and licenses: **radar** (memory-path educational red/blue lab for external “legit” radar vs detection) and **vision** (pure-pixel player outlining for accessibility research). They share a theme, not a codebase. Do not treat either subtree as a cheat product.

## Projects

| Path | Stack | License | Purpose |
|------|-------|---------|---------|
| [`radar/`](radar/) | C++20, CMake | MIT | Educational red/blue lab: T0–T4 external radar scars and counters (sim + optional Windows real backends) |
| [`vision/`](vision/) | Python 3.11–3.13, `uv` | AGPL-3.0-only | Pre-alpha package `cs2-vision-access` (CLI `cs2-vision`): outline visible players from screen/video pixels only |

## Quick start

**Radar** — see [`radar/README.md`](radar/README.md). Build under `radar/code/`:

```bash
cd radar/code
cmake -S . -B build -DLR_BUILD_TESTS=ON -DLR_BUILD_STRATEGY_LAB=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

**Vision** — see [`vision/README.md`](vision/README.md). From `vision/`:

```bash
cd vision
uv venv --python 3.12
uv sync --frozen --extra dev
uv run cs2-vision --help
```

## Layout (monorepo root)

```
.
├── README.md           This file
├── CONTRIBUTING.md     Monorepo contribution rules
├── SECURITY.md         Scope and reporting
├── NOTICE.md           Dual-license and third-party pointers
├── .gitignore          Root ignore union
├── .github/            Path-filtered CI + issue/PR templates
├── radar/              AC Research Lab (MIT)
└── vision/             CS2 Vision Access (AGPL-3.0-only)
```

Each subtree has its own `README`, `LICENSE`, `CONTRIBUTING`, `SECURITY`, and docs tree. Root files do not replace those.

## Status

Current local evidence and the publication boundary are recorded in
[`RELEASE_STATUS.md`](RELEASE_STATUS.md).

| Project | Maturity |
|---------|----------|
| **radar** | Mature educational lab: hundreds of strategy pairs across T0–T4 + crosscutting, sim-first, optional Windows real backends behind CMake flags |
| **vision** | Pre-alpha research tooling (0.1.0). CPU unit tests in CI; live overlay and multi-epoch training are operator-local |

## Not a cheat product

Both tracks are research / teaching / accessibility scaffolding.

- **vision** never opens the game process, reads game memory, or injects input. Capture is external pixels (screen, capture device, or local video).
- **radar** real mode is lab/read-oriented educational scaffolding (process attach, optional kernel/DMA paths under opt-in flags). Default CMake config is simulation. High-risk paths (sample vulnerable driver, DMA firmware examples, elevated real backends) are lab-only — see [`radar/SECURITY.md`](radar/SECURITY.md).

Operators own compliance with platform terms, anti-cheat policy, and local law.

## Docs and policy

| Doc | Role |
|-----|------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Monorepo PR rules; points into each subtree |
| [SECURITY.md](SECURITY.md) | Reporting and scope |
| [NOTICE.md](NOTICE.md) | Dual licensing |
| [RELEASE_STATUS.md](RELEASE_STATUS.md) | Current local evidence, Pages decision, and blockers |
| [radar/README.md](radar/README.md) · [radar/docs/INDEX.md](radar/docs/INDEX.md) | Radar curriculum and build |
| [vision/README.md](vision/README.md) · [vision/docs/](vision/docs/) | Vision install, CLI, architecture |
| [radar/CONTRIBUTING.md](radar/CONTRIBUTING.md) · [vision/CONTRIBUTING.md](vision/CONTRIBUTING.md) | Per-project contribution detail |
| [radar/SECURITY.md](radar/SECURITY.md) · [vision/SECURITY.md](vision/SECURITY.md) | Per-project security notes |

## Licensing

There is **no** single monorepo license. Each subtree keeps its own:

- `radar/` — MIT (`radar/LICENSE`)
- `vision/` — AGPL-3.0-only (`vision/LICENSE`); third-party notes in `vision/NOTICE.md` and [NOTICE.md](NOTICE.md)

Code or docs that sit only at the monorepo root (these policy files) are dual-applicable packaging; when in doubt, treat shared root text as documentation, not a relicense of either tree.
