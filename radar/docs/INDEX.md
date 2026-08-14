# Documentation index

## Start here

| Document | Description |
|----------|-------------|
| [README.md](../README.md) | Project overview, build, usage |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Code layout, World model, multi-step pattern |
| [BUILD-AND-TEST.md](BUILD-AND-TEST.md) | CMake options and tests |
| [CURRICULUM.md](CURRICULUM.md) | Suggested learning order |

## Concepts

| Document | Description |
|----------|-------------|
| [OVERVIEW.md](OVERVIEW.md) | Threat model, escalation ladder, design takeaways |
| [THREAT-TIERS.md](THREAT-TIERS.md) | T0–T4 delivery detail |
| [TIERED-COUNTERS.md](TIERED-COUNTERS.md) | Red vs blue matrix |
| [CROSSCUTTING.md](CROSSCUTTING.md) | Evasion, features, ops, structural themes |
| [SHARED.md](SHARED.md) | Shared libraries under `lib/` |
| [RESIDUAL-RESEARCH.md](RESIDUAL-RESEARCH.md) | VMX / BYOVD / DMA / SMM lab notes |

## Reference

| Document | Description |
|----------|-------------|
| [STRATEGY-CATALOG.md](STRATEGY-CATALOG.md) | Human index of strategy pairs |
| [TOOLS.md](TOOLS.md) | `strategy_lab`, labs, demos |
| [HISTORY.md](HISTORY.md) | Research arc and archive map |
| [../code/STRUCTURE.md](../code/STRUCTURE.md) | In-tree layout |
| [../CONTRIBUTING.md](../CONTRIBUTING.md) | How to add lessons |
| [../SECURITY.md](../SECURITY.md) | Lab safety |

## Tier lessons

| Tier | Folder |
|------|--------|
| T0 | [tiers/t0/](tiers/t0/) |
| T1 | [tiers/t1/](tiers/t1/) |
| T2 | [tiers/t2/](tiers/t2/) |
| T3 | [tiers/t3/](tiers/t3/) |
| T4 | [tiers/t4/](tiers/t4/) |

## Code-adjacent notes

| Document | Description |
|----------|-------------|
| [../code/docs/REAL-AC-LEARNINGS.md](../code/docs/REAL-AC-LEARNINGS.md) | Notes from real AC surfaces |
| [../code/docs/PAIR-TO-REAL-MAP.md](../code/docs/PAIR-TO-REAL-MAP.md) | Strategy → real backend map |
| [../code/docs/EVASION-OBFUSCATION.md](../code/docs/EVASION-OBFUSCATION.md) | Evasion / obfuscation notes |

## Archive

Frozen snapshots only — remediation/TODO ledgers and old rewrites. Do not treat them as the task list.

Live catalog truth is `./build/strategy_lab list` (pairs under `code/strategies/`). Live curriculum is the `docs/*.md` files above.

| Path | Description |
|------|-------------|
| [archive/](archive/) | Archive index |
| [archive/2026-07-21-pre-comprehensive/](archive/2026-07-21-pre-comprehensive/) | Pre-rewrite docs |
| [archive/2026-07-26-post-phase2/](archive/2026-07-26-post-phase2/) | Phase 2 remediation ledgers |
| [archive/2026-07-26-post-all-phases/](archive/2026-07-26-post-all-phases/) | Full remediation ledger series |
| [archive/2026-08-09-monolith-refactor/](archive/2026-08-09-monolith-refactor/) | >600 LoC split ledger |
