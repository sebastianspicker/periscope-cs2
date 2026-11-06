# Split ledger — extract the anti-cheat lab from `radar/`

Status: **executed (2026-11-06).** Phase commits on `main`: `c85290a`
(Phase 0, S1–S3), `7713581` (Phase 1, S4a–S4i), `4186c38` (Phase 2, S5–S7),
`eb7cf6d` (Phase 3, S8–S10), Phase 4 (S11–S13) = the commit that carries
this edit. Per-step evidence and recorded deviations:
[`docs/IMPLEMENTATION_PROGRESS.md`](IMPLEMENTATION_PROGRESS.md).
Owner request: a clean slate in `radar/` — the anti-cheat lab and its
simulation demos move one level up into their own `anti-cheat/` subtree;
`radar/` keeps the game-facing radar: real examples, adapters and live
reading. Final structure: `anti-cheat`, `lens`, `radar`, `vision`.

IDs below: `S#` = split step, grouped in phases. Every step names its
destination and its evidence path. Priority: P1 = must be right before any
move, P2 = move mechanics, P3 = post-move hygiene.

## Why this is not a directory rename

Measured dependency facts (2026-11-06, current tree):

| Fact | Evidence |
|---|---|
| The lab does **not** include `real/` | `scenarios`, `src/lab_components`, `src/simulation`, `src/domain`, `src/application/**` contain **zero** `real/` includes; the only hit in `scenarios` is a *comment* in `framework/main.cpp` |
| The real stack **does** include the lab | `adapters/real` includes `ac/types.hpp` (7 files), `ac/memory_backend.hpp` (2), and **14 `ac_sim/*` headers** in `adapters/real/application/radar_pipeline.hpp`; `radar_pipeline` instantiates all 14 evasion components |
| Team components need the sim world | `lab_components/**` includes `sim/world.hpp`, `ac/*`, `cs2/signatures.hpp`, `lab/*` |
| Hybrid demos need both | `cs2_radar/t0` (real: only `t0/main.cpp` includes `real/…` and links `ac_real_pipeline`; `t1–t4` are sim-only variants of the same family), `tier_comparison` (real+teams), `full_prototype` (real+sim) |

Therefore the only non-circular direction is **radar → anti-cheat** (radar
consumes the lab's foundation). The alternative — duplicating
the `ac/` types, `ac_sim/` and `sim/world*` into radar — is S1-alt below and
rejected for drift risk:
SIM mode of the real adapters must match the simulated world *exactly*.

## Non-negotiables

1. **Both subtrees stay MIT** (the lab is MIT today). The lab keeps no
   split-specific licensing; the adapted-upstream attribution (tiansongyu,
   cs2-dma, miata, …) stays with radar's code.
2. **The lab never imports real code** (enforced by the architecture check,
   see S10) — it must build with no Windows/real flags set.
3. **`radar/` keeps its lab flag independence**: real backends stay OFF by
   default; the anti-cheat dependency may only provide types, world and
   component libraries, never real adapters.
4. **No behaviour change in the split commit(s)**: same targets, same tests,
   same simulation results (187 pairs, 0 failures). The only CTest delta is
   `stack_posture_test` moving: radar goes 10/10 → 9/9, anti-cheat gains 1/1.
5. **One logical change per commit**: (a) scaffolding + seam, (b) lab
   extraction, (c) radar slimming, (d) docs/CI/licensing, (e) hygiene. The
   tree must verify green after each.

## Target layout

```
anti-cheat/                     # lab (MIT): teaches the tiers, owns the world
  README.md CONTRIBUTING.md SECURITY.md LICENSE NOTICE.md (states "no
  adapted upstreams; original lab code")
  CMakeLists.txt                # library + lab apps only
  cmake/                        # Options (lab), helpers, Domain/Simulation/Scenario/Team targets
  docs/                         # curriculum, lessons, catalogs, posture
  scenarios/                    # all strategy pairs + framework
  src/domain/{ac,server}        # shared AC vocabulary (types, memory_backend, risk, telemetry)
  src/simulation/{sim,cs2,fps,ac_sim,simulation}
  src/lab_components/{lab,teams}
  src/application/{analysis,detection,strategies,radar_pipeline}
  apps/demos/                   # sim-only demos (list at S4f)
  scripts/                      # lab verification scripts
  tests/                        # stack_posture + strategy harness glue
  data/                         # none today; any sim-side data would live here

radar/                          # product (MIT): reads the live game
  README.md CONTRIBUTING.md SECURITY.md LICENSE NOTICE.md
  CMakeLists.txt                # product root; consumes ../anti-cheat libs
  cmake/                        # Options (real flags), RealTargets, AppTargets, TestTargets, helpers
  src/domain/radar/             # our R1–R12 modules
  src/application/radar/        # webradar, tri_loader
  adapters/                     # all real backends (cs2 live reading incl.)
  apps/demos/                   # real + hybrid demos (remainder of S4f)
  examples/{drivers,firmware}/
  relay/ scripts/ data/cs2/ tests/ docs/DMA-SCATTER-POLICY.md
```

## Phase 0 — seam groundwork (P1)

- **S1 · Dependency seam.** `radar/CMakeLists.txt` gains
  `LR_ANTI_CHEAT_DIR` (default `../anti-cheat`, cache var, fatal error with
  a clear message when missing) and includes anti-cheat's *library-only*
  CMake (`DomainTargets`, `SimulationTargets`, `TeamSources`/Scenario
  libraries, helpers). Include roots: `anti-cheat/src` (so the existing
  `"ac/…"`, `"sim/…"`, `"ac_sim/…"` includes keep working verbatim) and
  `radar/adapters` (radar's existing include root, unchanged).
- **S1-alt (rejected unless S1 proves unworkable):** copy
  `ac/types.hpp`, `ac/memory_backend.hpp`, `ac_sim/**`, `sim/world*` into
  `radar/src/core/` and make anti-cheat include `radar/src` — flips the
  direction, duplicates the world. Revisit only if CMake coupling leaks
  (S1 acceptance: radar configures and builds with only
  `-DLR_ANTI_CHEAT_DIR` set).
- **S2 · Options split.** Real flags (`LR_ENABLE_REAL_*`, `LR_ENABLE_REAL_ALL`,
  `LR_ENABLE_SHELLCODE_INJECTION`) move to `radar/cmake/Options.cmake`.
  Product-side flags used only by the real pipeline sources (`LR_ENABLE_VEH`,
  `LR_BUILD_ID`) stay in radar's Options too. Lab flags
  (`LR_BUILD_STRATEGY_LAB`, `LR_BUILD_PROTOS`, `LR_BUILD_DUELS`, and the
  `LAB_ALLOW_*` simulation capability flags) stay in
  `anti-cheat/cmake/Options.cmake`. `LR_MODE` is not a CMake option — it is a
  runtime env var read by the demos and implemented in
  `adapters/real/mode/`; it moves with radar and needs no CMake action.
  Both trees keep the architecture checker's "declared OFF by default" rule
  for real flags. Note: all of these currently flow as compile definitions
  through the shared `lr_build_options` interface — after S3 owns that
  interface in anti-cheat, radar's real-side definitions
  (`LR_ENABLE_REAL_*`, `LR_ENABLE_VEH`, `LR_BUILD_ID`) must be appended by
  radar's own options file instead.
- **S3 · Shared helpers.** The library-side cmake files move with `git mv`
  in this phase — `DomainTargets.cmake`, `SimulationTargets.cmake`,
  `ScenarioTargets.cmake`, `TeamSources.cmake` → `anti-cheat/cmake/` — and
  `lr_library`/`lr_exe`/build-options are consolidated once in
  `anti-cheat/cmake/LrHelpers.cmake`; radar includes them via
  `LR_ANTI_CHEAT_DIR` and adds its own `adapters` include policy (today's
  rule that inner targets do not see `adapters/` must survive per-tree).
  Phase 0 also creates a minimal `anti-cheat/CMakeLists.txt` stub that only
  `include()`s the moved library files — the Phase 0 acceptance configures
  `-S anti-cheat`, and the full lab project definition waits for S7.
  `AppTargets.cmake`, `RealTargets.cmake`, `RealSources.cmake`,
  `TestTargets.cmake`, `build_keys.hpp.in` and `strip_rich_header.cmake`
  stay in `radar/cmake/` until S5–S7 split them.

Acceptance S1–S3: from the repo root, both
`cmake -S anti-cheat -B …` and `cmake -S radar -B radar/build/dev
-DLR_ANTI_CHEAT_DIR=../anti-cheat` configure; radar builds portable targets
with real flags OFF; `verify.py architecture` green.

## Phase 1 — lab extraction (P2)

Move with `git mv` in descending path order. Everything below leaves
`radar/` verbatim.

| S# | Move | Notes |
|---|---|---|
| S4a | `scenarios/strategies/**` → `anti-cheat/scenarios/strategies/` | 4.5 MB; strategy IDs untouched |
| S4b | `src/domain/ac`, `src/domain/server` → `anti-cheat/src/domain/` | `radar_domain` modules stay behind |
| S4c | `src/simulation/{sim,cs2,fps,ac_sim,simulation}` → `anti-cheat/src/simulation/` | the world + evasion components |
| S4d | `src/lab_components/**` → `anti-cheat/src/lab_components/` | teams/lab |
| S4e | `src/application/{analysis,detection,strategies,radar_pipeline.cpp,.hpp}` → `anti-cheat/src/application/` | `src/application/radar/` stays in radar |
| S4f | sim-only demos → `anti-cheat/apps/demos/`: `duel_t0–t4`, `proto_t*_red/blue`, `fps_demo`, `evasion_lab`, `features_lab`, `ops_lab`, `structural_lab`; plus the `strategy_lab` catalog runner target (its source `scenarios/strategies/framework/main.cpp` moves with S4a; re-registered per S5) | classification from include survey; `strategy_lab` links `ac_strategies` only |
| S4g | `docs/{CURRICULUM,CROSSCUTTING,OVERVIEW,RESIDUAL-RESEARCH,SHARED,STEALTH-POSTURE,STRATEGY-CATALOG,THREAT-TIERS,TIERED-COUNTERS,TOOLS}.md`, `docs/tiers/**` → `anti-cheat/docs/` | tier lessons are the curriculum |
| S4h | `scripts/{radar_regression.ps1,verify_features.sh,verify_features.ps1}` → `anti-cheat/scripts/` | they verify lab/demo surfaces |
| S4i | `tests/stack_posture_test.cpp` → `anti-cheat/tests/` | radar keeps its nine `radar_*` host tests (10 today incl. `stack_posture_test`) |

**S4e note — two same-named pipeline headers.** There are two distinct
`radar_pipeline.hpp` files, and both keep their names:
`src/application/radar_pipeline.hpp` (sim-side `FramePipeline`, includes only
`ac_sim/*`) moves with its sibling `radar_pipeline.cpp` — the `.cpp`'s
quote-include resolves to its own directory, so nothing on the radar side is
referenced. `adapters/real/application/radar_pipeline.hpp` (real-side
`RealFramePipeline`, `namespace radar::real_adapter`, 14 `ac_sim/*` includes)
stays in radar. The architecture checker asserts the real header keeps the
unambiguous `namespace radar::real_adapter` / `class RealFramePipeline` names
exactly so the pair stays distinguishable; S10 must re-point that rule at the
two new paths.

**Stays in radar:** `adapters/**`, `examples/{drivers,firmware}/**`,
`src/domain/radar/**`, `src/application/radar/**`, `apps/demos/` remainder
(`attach_probe`, `cs2_radar/**`, `desktop_dup_overlay`, `donor_worker`,
`entity_probe`, `full_prototype`, `gui_demo`, `hud_probe`, `icvar_listfind`,
`icvar_probe`, `icvar_probe2`, `live_radar{,_loop,_support}.cpp`,
`live_radar_support.hpp`, `radar_diag`, `radar_multi_tier`, `radar_t0–t4`,
`scale_probe`, `tier_comparison`, `uefi_firmware`, `radar_shared.hpp`),
`relay/`, `data/cs2/`, the cs2
signature/offset scripts (`cs2_sig_*`, `update_cs2_signatures.py`,
`resolve_dumper_ref.py`, `scripts/fixtures/`), `docs/{ARCHITECTURE,
BUILD-AND-TEST,INDEX,DMA-SCATTER-POLICY}.md`, `tests/radar_*`.

**Include-path note:** `apps/demos/radar_shared.hpp` includes `real/…`;
anti-cheat demos never use it. Hybrid demos keep
`#include "radar_shared.hpp"` — the demos directory stays an include root of
radar's app targets (unchanged by the split).

## Phase 2 — radar re-composition (P2)

- **S5 · AppTargets split.** `anti-cheat/cmake/AppTargets.cmake` keeps only
  S4f targets (links `ac_*` only, no real libs). `radar/cmake/AppTargets.cmake`
  keeps the real/hybrid targets exactly as today, still guarded by the real
  capability `if(TARGET …)` blocks. `strategy_lab` is registered by *both*
  AppTargets files (same source, one per tree configure — no conflict since
  the trees configure into separate build dirs): the radar-sim lane runs the
  binary from radar's build dir, so radar must keep registering it from
  `../anti-cheat/scenarios/strategies/framework/main.cpp` or `verify.py
  radar-sim` loses its 187-pair run.
- **S6 · TestTargets.** Anti-cheat registers `stack_posture_test` (+ any
  strategy harness shims); radar registers `radar_*_test` unchanged.
- **S7 · Root CMake.** `anti-cheat/CMakeLists.txt` gets the lab project
  (`project(ac_lab …)`), simulation/lab options, library targets, lab apps,
  `enable_testing()` + `add_subdirectory(tests)`. `radar/CMakeLists.txt`
  becomes `project(periscope_radar …)` (today it is oddly named
  `project(ac_sim …)`), requires the real options, includes
  `../anti-cheat` library cmake, builds `adapters`/product libs/tests.
  Helper/interface names keep the `ac_`/`lr_` prefixes (no target renames).

Acceptance S4–S7: `verify.py anti-cheat-sim` green (187 pairs, 0 failures;
CTest 1/1) and `verify.py radar-sim` green (CTest 9/9 — 10/10 before the
split, `stack_posture_test` moved; 187 pairs from the
radar lane's `strategy_lab all`); no target name changed; both trees build
from clean build dirs.

## Phase 3 — docs, notices, CI (P2/P3)

- **S8 · README/NOTICE/SECURITY split.**
  - `anti-cheat/README.md`: from `radar/README.md`'s lab half; a new
    `anti-cheat/docs/INDEX.md` becomes its curriculum index (radar keeps its
    own `docs/INDEX.md`). `radar/README.md` becomes product-only
    (build, offsets, live modes, webradar/relay, examples).
  - `radar/NOTICE.md`: keeps all adapted-upstream attributions (they concern
    radar code). `anti-cheat/NOTICE.md`: none needed (original lab code), but
    add one for completeness stating that.
  - `radar/SECURITY.md`: keeps real-path risk table + X4 field note.
    `anti-cheat/SECURITY.md`: lab-only posture (simulation, sample driver
    exclusions point at `../radar/examples/…`).
  - Root `README.md`, `SECURITY.md`, `NOTICE.md`, `CONTRIBUTING.md`,
    `docs/ARCHITECTURE.md`: four-tree tables/maps; anti-cheat row first.
  - Link re-pointing: radar's remaining docs link into the moved set —
    `radar/README.md` links `docs/STRATEGY-CATALOG.md` and
    `docs/CURRICULUM.md`, and the staying `radar/docs/INDEX.md` links most
    of the moved lab docs. S8 re-points those to `../anti-cheat/docs/…`
    (S13's link check verifies).
- **S9 · CI + lanes.** New `.github/workflows/anti-cheat-ci.yml` (paths
  `anti-cheat/**`), radar-ci path filters gain `anti-cheat/**`; `verify.py`
  gains `anti-cheat-sim`, and both `all` order and docs list it. The
  real-capability Windows job stays dispatch-only on radar-ci.
- **S10 · Architecture checker.** `scripts/check_architecture.py` hardcodes
  several radar paths; all of them are re-pointed or duplicated for the new
  tree:
  - *New `check_anti_cheat`:* no `real/` (or `adapters/real/`) includes and
    no `#if 0` blocks anywhere under `anti-cheat/src` + `anti-cheat/scenarios`
    (mirrors the current `radar/src` + `radar/scenarios` rule); required
    files present; lab layering — domain may not import
    `sim|ac_sim|cs2|fps|lab|teams|application|scenarios|adapters` — the
    current `radar/src/domain` rule re-pointed to `anti-cheat/src/domain`.
  - *Radar rules keep:* domain purity for `src/domain/radar` (same forbidden
    list, so `ac/…` stays out — currently true in practice, now enforced);
    the "inner targets must not see `adapters/`" rule over
    `DomainTargets`/`SimulationTargets`/`ScenarioTargets` — re-pointed to
    anti-cheat's cmake copies; `LR_ENABLE_REAL_*` "declared OFF by default"
    over *both* Options files; no `file(GLOB)` in either tree's cmake; the
    simulation host-operation-marker and `shellcode.cpp` rules follow
    `src/simulation` into anti-cheat; the real-vs-sim pipeline
    disambiguation rule (`RealFramePipeline` / `namespace
    radar::real_adapter`) re-pointed to the two new header paths (see the
    S4e note); `adapters` keeps its allowance for `ac/…`/`sim/…` includes
    (already true).
  - The Vision/Lens sections of the checker are untouched.

## Phase 4 — hygiene (P3)

- **S11 · Wording sweep.** “radar” in lab docs → “lab/anti-cheat”; keep the
  product name Radar in product docs. Fix
  `docs/decisions/0001-independent-products.md` (“two projects” — the repo
  grows a third product subtree).
- **S12 · Progress ledger.** Add the split as its own domain row block;
  move R-item evidence paths that changed (none: radar modules stay).
- **S13 · Link/doc check**, `actionlint`, full `verify.py all`, clean tree.

## Acceptance (per phase, all required)

| Phase | Gate |
|---|---|
| 0 | architecture ✓; both trees configure; radar portable build with real OFF |
| 1 | `verify.py anti-cheat-sim` and `radar-sim` ✓ from clean dirs; 187/0; CTest 1/1 (anti-cheat) + 9/9 (radar, was 10/10) |
| 2 | as phase 1 plus: no target renamed; real-capability configure works (dispatch job re-run) |
| 3 | actionlint ✓; docs link check ✓; CI path matrices reviewed; no stale path hits (`grep -rn "radar/src/simulation\|radar/scenarios"`) |
| 4 | `verify.py all` ✓ (architecture, anti-cheat, radar, vision, lens); clean tree |

## Risk register

| # | Risk | Mitigation |
|---|---|---|
| 1 | CMake subdirectory cycle or duplicate target | S1 seam is one-way; anti-cheat never includes radar cmake; target names asserted unique |
| 2 | Include-root leakage (lab accidentally seeing `real/`) | S10 checker rule; include dirs per target, not global |
| 3 | Radar platform job breaks (Windows real-capability) | run the dispatch job before merging; it consumes both trees by design |
| 4 | CI blind spots after path moves | S9 updates both path filters; `all` includes both lanes |
| 5 | Name confusion (two `radar_pipeline` headers, “radar” names inside anti-cheat) | acceptable and already guarded: the lab keeps sim `FramePipeline` (`src/application/radar_pipeline.*`), radar keeps `RealFramePipeline` (`adapters/real/application/`), and the S10 checker rule enforces the unambiguous real-side names; documented in `anti-cheat/docs/INDEX.md` |
| 6 | Docs/links rot | S11 sweep + link check; acceptance greps |
| 7 | Review noise | five focused commits (phase commits), each green |

## Rollback

Each phase lands as one commit; `git revert` of a phase restores the prior
tree because no history is rewritten. `git mv` preserves per-file history,
so a selective re-merge is possible.

## Open decisions (default = recommended, applied unless overruled)

1. **Dependency direction** — default S1 (radar → anti-cheat). Alt S1-alt
   documented in Phase 0.
2. **`ac_sim` placement** — default stays in anti-cheat (used only by the
   real pipeline, but conceptually lab components; keeping it there avoids a
   second dependency direction).
3. **`examples/firmware` host simulator** — default stays in radar: it is a
   DMA example first, even though it contains a small host sim.
4. **Directory name** — default `anti-cheat/` (literal `ac_lab` CMake
   project, `ac_` targets kept).
5. **`cs2_radar` demos** — default stay in radar as one family (`t0` is the
   real build and the family's point; `t1–t4` are its sim-only variants),
   with the sim fallback consuming anti-cheat libs.

## Out of scope

- Any behaviour/feature change; the R1–R12 modules are untouched.
- Push/publication (still local-only by standing decision).
- The `ac/` namespace and `lr_`/`ac_` target names (no renames in this task).
