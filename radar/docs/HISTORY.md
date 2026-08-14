# History

Short research arc. Old ledgers and snapshots live under `archive/`.

## Arc

1. **Scope** — External “legit” radar spans usermode RPM → syscall → kernel/BYOVD → personal HV → off-box DMA. Durable defense reduces client trust and leans on server authority plus multi-signal residuals.
2. **Tiered lab** — T0–T4 team libraries, strategy pairs, and demos that share one scar arena (`sim::World`).
3. **Catalog growth** — Delivery, evasion, feature, detection, and structural pairs registered under `code/strategies/`. Live count is whatever `strategy_lab list` prints (hundreds of folders; registry drives runnable IDs).
4. **Quality bar** — Multi-step red, multi-reason blue, full_tests against shipped entry points. Single-flag lessons are not enough.
5. **2026-07 restructure** — Flattened layout: `lib/`, `teams/`, `strategies/{t0..t4,crosscutting}/`, `demos/`, `tests/`. Docs cleaned; long-lived ledgers archived.
6. **Real backends** — `lib/real/` grew Windows (and some Linux) paths for attach, overlay, kernel scaffolding, VMX, DMA, UEFI-shaped lab code. Optional behind CMake flags.
7. **2026-08-09 monolith split** — Project-owned sources capped at 600 physical lines; refactor ledger archived under `archive/2026-08-09-monolith-refactor/`.
8. **2026-08-09 hygiene** — Build trees, stray `.obj`/`.log` dumps, and tool config junk removed; live docs aligned with the tree. Old remediation/TODO ledgers stay under `archive/` only.
9. **2026-08 Periscope monorepo** — Radar lives under the Periscope umbrella (`periscope/radar`) alongside `vision/` (pixel-based outlining; separate stack and license). No nested product packaging beyond this folder yet.

## Archives

| Path | Content |
|------|---------|
| `archive/2026-07-21-pre-comprehensive/` | Docs before the full-lab rewrite |
| `archive/2026-07-26-post-phase2/` | Phase 2 TODO / detection / radar ledgers |
| `archive/2026-07-26-post-all-phases/` | Remediation ledger series v1–v9, progress ledgers |
| `archive/2026-08-09-monolith-refactor/` | Monolith split inventory |
| `archive/shapes/`, `archive/matrices/`, `archive/analysis/` | Early design and matrix notes |
| `archive/simulation-model.md` | Retired; superseded by `ARCHITECTURE.md` |

Live docs under `docs/*.md` and the source tree win over anything in `archive/`. Live strategy catalog = `strategy_lab list`.
