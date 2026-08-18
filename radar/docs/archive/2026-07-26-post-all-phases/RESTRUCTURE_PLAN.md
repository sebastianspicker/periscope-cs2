# Restructure Plan — anti-cheat-legit-radar

**Status:** ready to execute  
**Goal:** consolidate duplicate/nested folders with **minimal breakage** while keeping `cmake` + `ctest` green.  
**Non-goals this pass:** flattening virtual includes, rewriting strategy bodies, full cs2_radar DRY, registry renumbering.

---

## 0. Decisions (locked)

| Decision | Choice |
|----------|--------|
| Tier outer folders | Rename long names → short: `t0`…`t4` |
| Virtual includes `"tN/red/…"` | **Keep** physical `red/include/tN/red/` layout |
| Team libs + duel narratives | **Keep separate** per tier (unique) |
| Strategy pair folders | **Keep flat** under `strategies/` / crosscutting (already good) |
| `proto/` sibling | **Eliminate** — fold `demos/cs2_radar` into `demos/cs2_radar` |
| cs2_radar ×5 clones | **Relocate only** this pass; shared adaptive shell is a later pass |
| Orphans not in CMake | Flatten to strategy-shaped folders + wire sources |
| Tests in `shared/lab/src` | Move all test TUs into `code/verif/` |
| Docs ledgers | Move under `docs/meta/` (this file lives there) |
| Incomplete pairs | Add missing `pair.cpp` for `gaming_chair`, `skin_changer` |
| Registry ID collisions | Document only — fix in a follow-up pass |

---

## 1. Target tree (ASCII)

```text
anti-cheat-legit-radar/
├── README.md
├── ARCHIVE_LEDGER.md                 # leave at root (historical pointer)
├── code/
│   ├── CMakeLists.txt
│   ├── cmake/
│   ├── README.md
│   ├── PROTOTYPES.md
│   ├── shared/                       # KEEP include/<ns>/ pattern
│   │   ├── common/  sim/  blue/  server/  depth/  fps/  cs2/  lab/
│   │   └── lab/
│   │       ├── include/lab/…
│   │       └── src/                  # library only (no *_tests.cpp)
│   ├── tiers/
│   │   ├── t0/                       # was t0
│   │   │   ├── red/
│   │   │   │   ├── include/t0/red/   # DO NOT flatten this pass
│   │   │   │   └── src/
│   │   │   ├── blue/
│   │   │   │   ├── include/t0/blue/
│   │   │   │   └── src/
│   │   │   ├── strategies/<name>/    # pair.cpp + red/blue_example.*
│   │   │   ├── demos/
│   │   │   │   ├── duel/main.cpp
│   │   │   │   ├── proto_red/main.cpp
│   │   │   │   ├── proto_blue/main.cpp
│   │   │   │   └── cs2_radar/        # was demos/cs2_radar/
│   │   │   │       ├── cs2_radar.cpp
│   │   │   │       ├── cs2_radar.hpp
│   │   │   │       └── main.cpp
│   │   │   ├── LESSON.md
│   │   │   └── README.md
│   │   ├── t1/                       # was t1
│   │   ├── t2/                       # was t2 (+ red/driver_abi stays)
│   │   ├── t3/                       # was t3 (+ red/hv_abi stays)
│   │   └── t4/                       # was t4
│   ├── crosscutting/
│   │   ├── README.md
│   │   ├── evasion/
│   │   │   ├── <pair>/…
│   │   │   └── demos/evasion_lab/main.cpp
│   │   ├── features/
│   │   │   ├── <pair>/…              # all pairs incl. gaming_chair, skin_changer
│   │   │   ├── behavioral_decoupler/ # was orphan include/+src/
│   │   │   │   ├── behavioral_decoupler.hpp
│   │   │   │   ├── behavioral_decoupler.cpp
│   │   │   │   └── pair.cpp          # new thin registry wire (optional catalog)
│   │   │   └── demos/features_lab/main.cpp
│   │   ├── ops/
│   │   │   ├── <pair>/…
│   │   │   ├── adaptive_strategy/    # was ops/red_strategy deep nest
│   │   │   │   ├── adaptive_strategy.hpp
│   │   │   │   ├── adaptive_strategy.cpp
│   │   │   │   └── pair.cpp
│   │   │   └── demos/ops_lab/main.cpp
│   │   └── structural/
│   │       ├── <pair>/…
│   │       └── demos/structural_lab/main.cpp
│   ├── tools/
│   │   ├── strategy_lab/
│   │   └── fps_demo/
│   └── verif/                        # ALL test executables live here
│       ├── smoke_test.cpp
│       ├── team_api_tests.cpp
│       ├── depth_tests.cpp
│       ├── fps_tests.cpp
│       ├── t0_full_tests.cpp … t4_full_tests.cpp
│       ├── evasion_full_tests.cpp
│       ├── features_full_tests.cpp
│       ├── ops_full_tests.cpp
│       ├── structural_full_tests.cpp
│       ├── shared_full_tests.cpp
│       ├── tools_full_tests.cpp
│       ├── full_duel_test.cpp        # already here
│       ├── archive_active_set_test.cpp
│       └── classification_contract_test.cpp
└── docs/
    ├── INDEX.md                      # updated
    ├── ARCHITECTURE.md               # absorbs simulation-model
    ├── OVERVIEW.md  CURRICULUM.md  THREAT-TIERS.md  …
    ├── BUILD-AND-TEST.md  SHARED.md  CROSSCUTTING.md  TOOLS.md
    ├── STRATEGY-CATALOG.md  TIERED-COUNTERS.md  RESIDUAL-RESEARCH.md
    ├── HISTORY.md
    ├── meta/
    │   ├── RESTRUCTURE_PLAN.md       # this file
    │   ├── IMPL_LEDGER.md
    │   ├── PROGRESS_LEDGER.md
    │   └── LEDGER_PERISCOPE_LEARNINGS.md
    └── archive/
        ├── README.md
        ├── 2026-07-21-pre-comprehensive/…
        ├── shapes/                   # was undated blue_shapes + red_shapes
        │   ├── blue_shapes/
        │   └── red_shapes/
        ├── matrices/                 # was undated sibling
        ├── analysis/                 # was docs/analysis orphan
        │   └── strategy-classification.md
        └── simulation-model.md       # retired from docs/architecture/
```

**Per-tier skeleton after pass (identical shape, unique content):**

```text
tiers/tN/
  red/{include/tN/red, src[, driver_abi|hv_abi]}
  blue/{include/tN/blue, src}
  strategies/<pair>/
  demos/{duel, proto_red, proto_blue, cs2_radar}/
  LESSON.md  README.md
```

---

## 2. Ordered phases (exact moves)

### Phase 0 — Baseline (no moves)

```bash
cd code && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

Record pass/fail count. Do not proceed if baseline is red for reasons unrelated to this plan.

---

### Phase 1 — Docs hygiene (zero code impact)

| From | To |
|------|-----|
| `docs/meta/IMPL_LEDGER.md` | `docs/meta/IMPL_LEDGER.md` |
| `docs/meta/PROGRESS_LEDGER.md` | `docs/meta/PROGRESS_LEDGER.md` |
| `docs/meta/LEDGER_PERISCOPE_LEARNINGS.md` | `docs/meta/LEDGER_PERISCOPE_LEARNINGS.md` |
| `docs/archive/simulation-model.md` | `docs/archive/simulation-model.md` |
| `docs/archive/analysis/strategy-classification.md` | `docs/archive/analysis/strategy-classification.md` |
| `docs/archive/blue_shapes/` | `docs/archive/shapes/blue_shapes/` |
| `docs/archive/red_shapes/` | `docs/archive/shapes/red_shapes/` |
| `docs/archive/matrices/` | `docs/archive/matrices/` (stay; ensure under archive only) |

Then:

1. Delete empty `docs/architecture/` and `docs/analysis/` if empty.
2. Fold one-paragraph pointer into `docs/ARCHITECTURE.md` § Simulation model → “see archive/simulation-model.md (superseded summary)”.
3. Update `docs/INDEX.md`: add `meta/`, drop ledgers from curriculum list, point analysis/shapes to archive.

**Verify:** no build step required.

---

### Phase 2 — Shorten tier outer folders (primary consolidation)

| From | To |
|------|-----|
| `code/tiers/t0` | `code/tiers/t0` |
| `code/tiers/t1` | `code/tiers/t1` |
| `code/tiers/t2` | `code/tiers/t2` |
| `code/tiers/t3` | `code/tiers/t3` |
| `code/tiers/t4` | `code/tiers/t4` |

**Do not touch** inner `include/tN/{red,blue}/` trees.

#### Path rewrites (same phase)

**`code/CMakeLists.txt`**

```cmake
# before
set(T0 tiers/t0)
set(T1 tiers/t1)
set(T2 tiers/t2)
set(T3 tiers/t3)
set(T4 tiers/t4)

# after
set(T0 tiers/t0)
set(T1 tiers/t1)
set(T2 tiers/t2)
set(T3 tiers/t3)
set(T4 tiers/t4)
```

Global replace inside `AC_STRATEGY_PAIRS` and any other hard-coded paths:

| Pattern | Replacement |
|---------|-------------|
| `tiers/t0/` | `tiers/t0/` |
| `tiers/t1/` | `tiers/t1/` |
| `tiers/t2/` | `tiers/t2/` |
| `tiers/t3/` | `tiers/t3/` |
| `tiers/t4/` | `tiers/t4/` |

**C++ `#include "tiers/tN_longname/…"`** (tests + composition strategy):

| Files | Change |
|-------|--------|
| `code/verif/t0_full_tests.cpp` … `t4_full_tests.cpp` | long → short tier paths |
| `code/verif/team_api_tests.cpp` | same |
| `code/verif/evasion_full_tests.cpp` etc. if any | same |
| `code/crosscutting/structural/composition_radar_loop/{red,blue}_example.cpp` | `tiers/t0/strategies/…` → `tiers/t0/strategies/…` |

**Docs / root that name long paths** (content edit, not git-mv):

- `README.md`, `code/README.md`, `code/PROTOTYPES.md`
- `docs/OVERVIEW.md`, `docs/ARCHITECTURE.md`, `docs/THREAT-TIERS.md`, `docs/SHARED.md`, `docs/BUILD-AND-TEST.md`, `docs/CROSSCUTTING.md`, `docs/STRATEGY-CATALOG.md`, `docs/CURRICULUM.md`, `docs/TOOLS.md`
- `docs/meta/*` ledgers (after Phase 1 move)
- **Do not edit** frozen `docs/archive/2026-07-21-pre-comprehensive/**` (historical)

**Verify Phase 2:**

```bash
cd code && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

---

### Phase 3 — Fold `demos/cs2_radar` into `demos/cs2_radar`

Per tier `t0`…`t4`:

| From | To |
|------|-----|
| `code/tiers/tN/demos/cs2_radar/*` | `code/tiers/tN/demos/cs2_radar/*` |

Remove empty `code/tiers/tN/proto/` after move.

**CMake** (uses `${TN}` so only the relative tail changes):

```cmake
# before
${T0}/demos/cs2_radar/cs2_radar.cpp
${T0}/demos/cs2_radar/main.cpp
# include: ${T0}/demos/cs2_radar

# after
${T0}/demos/cs2_radar/cs2_radar.cpp
${T0}/demos/cs2_radar/main.cpp
# include: ${T0}/demos/cs2_radar
```

Same for T1–T4. T4’s extra includes of T0 red/blue stay as `${T0}/red/include` etc.

**Docs:** `docs/ARCHITECTURE.md`, `docs/OVERVIEW.md`, `docs/SHARED.md`, `code/README.md` — replace `demos/cs2_radar` with `demos/cs2_radar`.

**Not this phase:** merging the five near-clone adaptive loops into one shared engine (follow-up).

**Verify Phase 3:** build + run `cs2_radar_t0`…`t4` smoke (or full ctest).

---

### Phase 4 — Tests out of `shared/lab`

Move test translation units only (leave lab library sources).

| From | To |
|------|-----|
| `code/verif/smoke_test.cpp` | `code/verif/smoke_test.cpp` |
| `code/verif/team_api_tests.cpp` | `code/verif/team_api_tests.cpp` |
| `code/verif/depth_tests.cpp` | `code/verif/depth_tests.cpp` |
| `code/verif/fps_tests.cpp` | `code/verif/fps_tests.cpp` |
| `code/verif/t0_full_tests.cpp` | `code/verif/t0_full_tests.cpp` |
| `code/verif/t1_full_tests.cpp` | `code/verif/t1_full_tests.cpp` |
| `code/verif/t2_full_tests.cpp` | `code/verif/t2_full_tests.cpp` |
| `code/verif/t3_full_tests.cpp` | `code/verif/t3_full_tests.cpp` |
| `code/verif/t4_full_tests.cpp` | `code/verif/t4_full_tests.cpp` |
| `code/verif/evasion_full_tests.cpp` | `code/verif/evasion_full_tests.cpp` |
| `code/verif/features_full_tests.cpp` | `code/verif/features_full_tests.cpp` |
| `code/verif/ops_full_tests.cpp` | `code/verif/ops_full_tests.cpp` |
| `code/verif/structural_full_tests.cpp` | `code/verif/structural_full_tests.cpp` |
| `code/verif/shared_full_tests.cpp` | `code/verif/shared_full_tests.cpp` |
| `code/verif/tools_full_tests.cpp` | `code/verif/tools_full_tests.cpp` |

**Already in verif (no move):** `full_duel_test.cpp`, `archive_active_set_test.cpp`, `classification_contract_test.cpp`.

**CMake** — every test source path:

| Old | New |
|-----|-----|
| `verif/smoke_test.cpp` | `verif/smoke_test.cpp` |
| `verif/team_api_tests.cpp` | `verif/team_api_tests.cpp` |
| `shared/lab/src/*_full_tests.cpp` | `verif/*_full_tests.cpp` |
| `verif/depth_tests.cpp` | `verif/depth_tests.cpp` |
| `verif/fps_tests.cpp` | `verif/fps_tests.cpp` |

`target_include_directories(... PRIVATE ${CMAKE_SOURCE_DIR})` stays (tests still include `tiers/t0/strategies/...`).

**Docs:** `docs/ARCHITECTURE.md` lab note: “tests live under `code/verif/`”.

**Verify Phase 4:** full ctest.

---

### Phase 5 — Orphans + incomplete pairs

#### 5a. `behavioral_decoupler` (orphan, not in CMake)

| From | To |
|------|-----|
| `code/crosscutting/features/include/features/behavioral_decoupler.hpp` | `code/crosscutting/features/behavioral_decoupler/behavioral_decoupler.hpp` |
| `code/crosscutting/features/src/behavioral_decoupler.cpp` | `code/crosscutting/features/behavioral_decoupler/behavioral_decoupler.cpp` |

Remove empty `features/include/` and `features/src/`.

**Code edit:**

- In `.cpp`: `#include "behavioral_decoupler.hpp"` (local).
- In `.hpp`: keep public API; if anything external expected `"features/behavioral_decoupler.hpp"`, add a one-line shim later — currently no consumers in tree.
- Add `pair.cpp` (thin apply/detect wrapper or no-op catalog entry) **or** add both sources to `ac_strategies` / small `ac_features` lib in CMake without registry until IDs are cleaned.

**Minimal CMake wire (preferred this pass):** append sources to `ac_lab` or `ac_strategies` and set include to the pair folder; do **not** invent a new registry ID until Phase-registry follow-up.

#### 5b. `ops/red_strategy` deep nest (orphan)

| From | To |
|------|-----|
| `code/crosscutting/ops/red_strategy/include/ops/red/adaptive_strategy.hpp` | `code/crosscutting/ops/adaptive_strategy/adaptive_strategy.hpp` |
| `code/crosscutting/ops/red_strategy/src/adaptive_strategy.cpp` | `code/crosscutting/ops/adaptive_strategy/adaptive_strategy.cpp` |

Remove empty `red_strategy/` tree.

**Code edit:**

- `.cpp`: `#include "adaptive_strategy.hpp"`.
- Wire `.cpp` into CMake (`ac_strategies` or `ac_sim` consumer) so the TU is no longer dead.
- Optional thin `pair.cpp` later; not required for build green if linked as library source.

#### 5c. Incomplete pairs

| Folder | Action |
|--------|--------|
| `crosscutting/features/gaming_chair/` | Add `pair.cpp` (copy pattern from `aim_humanization/pair.cpp`; pick free ID only after registry audit — **for this pass** compile-only: list `pair.cpp` in `AC_STRATEGY_PAIRS` only if factory is also registered, else leave sources as today and add `pair.cpp` + register in follow-up) |
| `crosscutting/features/skin_changer/` | Same |

**Decisive this pass:** create `pair.cpp` for both using **unused** entry numbers **not** present in `registry.cpp` today (registry tops at 72; many pair files use higher/colliding numbers). To avoid worsening collisions:

1. Add `pair.cpp` with factories `entry_73_skin_changer` and `entry_74_gaming_chair` **only if** those symbols are free after grep.
2. Register both in `registry.cpp` catalog table.
3. Append `pair.cpp` + examples to `AC_STRATEGY_PAIRS` (skin_changer/gaming_chair currently list only blue/red without pair).

If free-ID check fails, still add `pair.cpp` with local `run` but **do not** register until follow-up — and keep CMake listing only red/blue as today.

#### 5d. Crosscutting demos (no move)

Keep `crosscutting/*/demos/*_lab/` — already correct and referenced by CMake.

**Verify Phase 5:** full build + ctest; `strategy_lab all --quiet` if pairs registered.

---

### Phase 6 — Doc path sweep + INDEX

After code moves stabilize:

1. Grep active docs (exclude `docs/archive/2026-07-21-pre-comprehensive/`) for old long tier names and `demos/cs2_radar` / `shared/lab/src/*_tests`.
2. Update `docs/INDEX.md` table to match target tree.
3. Update `docs/BUILD-AND-TEST.md` binary/path table if needed.
4. One-line note in `docs/HISTORY.md`: “2026-07-25 tree restructure: short tier dirs, demos/cs2_radar, verif tests, docs/meta ledgers.”

---

### Phase 7 — Final verification

```bash
cd path/to/periscope/radar/code
rm -rf build
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
# optional demos
./build/strategy_lab all --quiet
./build/cs2_radar_t0 ; ./build/duel_t0
```

---

## 3. Files that need path rewrites

### Build system

| File | What |
|------|------|
| `code/CMakeLists.txt` | `T0`…`T4` vars; all `AC_STRATEGY_PAIRS` tier paths; cs2_radar sources/includes; all test source paths; orphan source adds |

### C++ includes / sources

| File | What |
|------|------|
| `code/verif/t*_full_tests.cpp` (after move) | `tiers/tN_longname/strategies/…` → `tiers/tN/strategies/…` |
| `code/verif/team_api_tests.cpp` | same |
| `code/verif/*` any remaining long paths | same |
| `code/crosscutting/structural/composition_radar_loop/red_example.cpp` | tier strategy include path |
| `code/crosscutting/structural/composition_radar_loop/blue_example.cpp` | same |
| `code/crosscutting/features/behavioral_decoupler/*` | local include after flatten |
| `code/crosscutting/ops/adaptive_strategy/*` | local include after flatten |
| `code/tools/strategy_lab/src/registry.cpp` | only if new pair factories registered (5c) |

### Docs / README (active)

| File | What |
|------|------|
| `README.md`, `code/README.md`, `code/PROTOTYPES.md` | tier dir names, demos layout |
| `docs/ARCHITECTURE.md` | monorepo map; fold simulation-model pointer |
| `docs/OVERVIEW.md` | tier path table |
| `docs/THREAT-TIERS.md` | Path: lines |
| `docs/SHARED.md`, `docs/CROSSCUTTING.md`, `docs/TOOLS.md` | paths |
| `docs/BUILD-AND-TEST.md` | source locations |
| `docs/STRATEGY-CATALOG.md`, `docs/CURRICULUM.md` | path strings |
| `docs/INDEX.md` | meta/ + archive layout |
| `docs/HISTORY.md` | one-line restructure note |
| `docs/meta/*_LEDGER*.md` | internal path strings (best-effort) |

### Frozen (do **not** rewrite)

- `docs/archive/2026-07-21-pre-comprehensive/**`
- `code/build/**` (regenerate)

---

## 4. What NOT to change this pass

1. **Physical** `red/include/tN/red/` and `blue/include/tN/blue/` nesting — virtual includes stay.
2. **Merging** red/blue team libraries across tiers.
3. **Collapsing** 91 strategy folders or deleting boilerplate `pair.cpp` bodies.
4. **Full DRY** of `cs2_radar` adaptive loop into one shared implementation.
5. **Registry renumber** / collision purge for entries 73+ (aliases like `entry_84_lag_switch`, duplicate IDs across pairs) — separate catalog pass.
6. **Introducing GLOB** in CMake — keep explicit lists.
7. **shared/** `include/<ns>/` layout.
8. **Rewriting** pedagogical LESSON.md content (paths only if broken).
9. **Moving** crosscutting category boundaries (evasion/features/ops/structural).
10. **Deleting** unique ABI trees (`t2/red/driver_abi`, `t3/red/hv_abi`).

---

## 5. Verification steps

```bash
# Clean configure + build
cd path/to/periscope/radar/code
cmake -S . -B build
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Full test suite
ctest --test-dir build --output-on-failure

# Spot-check demos / catalog
./build/strategy_lab all --quiet
./build/proto_t0_red
./build/duel_t0
./build/cs2_radar_t0

# Sanity: no stale long paths in active tree
rg -n 't0|t1|t2|demos/cs2_radar' \
  --glob '!docs/archive/2026-07-21-pre-comprehensive/**' \
  --glob '!code/build/**' \
  path/to/periscope/radar
# expect: only HISTORY / intentional archive mentions
```

Per-phase gate: **do not start next phase if ctest is red** after a code-touching phase (2–5).

---

## 6. Success criteria

| Criterion | Measure |
|-----------|---------|
| Build green | `cmake --build` exit 0 |
| Tests green | `ctest` all pass (same or better than Phase 0 baseline) |
| Short tiers | Only `code/tiers/t{0,1,2,3,4}/` exist (no `tN_longname`) |
| Single demos root | No `code/tiers/tN/proto/`; cs2_radar under `demos/cs2_radar/` |
| Virtual includes intact | `#include "t0/red/…"` still compiles via `target_include_directories(.../red/include)` |
| Lab purity | `shared/lab/src/` has **zero** `*_tests.cpp` / `smoke_test.cpp` |
| Verif home | All test TUs under `code/verif/` |
| Orphans gone | No `features/include`+`src` nest; no `ops/red_strategy/` nest |
| Docs | Ledgers under `docs/meta/`; INDEX lists meta + archive shapes; curriculum top-level uncluttered |
| Strategy pairs flat | Unchanged pair layout under tiers/crosscutting |
| No GLOB | CMake still explicit |
| Catalog runnable | `strategy_lab all --quiet` exits 0 |

---

## 7. Follow-ups (explicitly out of scope)

1. Extract shared `cs2::radar_proto` types + adaptive loop; per-tier technique tables only.
2. Registry: unique IDs 01…N, drop aliases, register every compiled pair.
3. Optional later: `tiers/tN` display names only in LESSON titles (`T0 Usermode RPM`), not folder names.
4. Consider `pair_util` codegen for boilerplate — not a tree move.

---

## Appendix A — Machine-readable `git mv` list

Run from repo root. Execute phases in order. Create parent dirs before moves when needed.

```bash
#!/usr/bin/env bash
# restructure_moves.sh — Phase 1, 2, 3, 4, 5a, 5b only (no content rewrites)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# ── Phase 1: docs ──────────────────────────────────────────
mkdir -p docs/meta docs/archive/shapes docs/archive/analysis

git mv docs/meta/IMPL_LEDGER.md              docs/meta/IMPL_LEDGER.md
git mv docs/meta/PROGRESS_LEDGER.md          docs/meta/PROGRESS_LEDGER.md
git mv docs/meta/LEDGER_PERISCOPE_LEARNINGS.md docs/meta/LEDGER_PERISCOPE_LEARNINGS.md

git mv docs/archive/simulation-model.md docs/archive/simulation-model.md
git mv docs/archive/analysis/strategy-classification.md docs/archive/analysis/strategy-classification.md

git mv docs/archive/blue_shapes docs/archive/shapes/blue_shapes
git mv docs/archive/red_shapes  docs/archive/shapes/red_shapes
# matrices already under docs/archive/matrices — no move

# remove empty dirs if git left them
rmdir docs/architecture 2>/dev/null || true
rmdir docs/analysis 2>/dev/null || true

# ── Phase 2: short tier names ──────────────────────────────
git mv code/tiers/t0 code/tiers/t0
git mv code/tiers/t1 code/tiers/t1
git mv code/tiers/t2 code/tiers/t2
git mv code/tiers/t3   code/tiers/t3
git mv code/tiers/t4 code/tiers/t4

# ── Phase 3: demos/cs2_radar → demos/cs2_radar ─────────────
for t in t0 t1 t2 t3 t4; do
  git mv "code/tiers/$t/demos/cs2_radar" "code/tiers/$t/demos/cs2_radar"
  rmdir "code/tiers/$t/proto" 2>/dev/null || true
done

# ── Phase 4: tests → verif ─────────────────────────────────
git mv code/verif/smoke_test.cpp            code/verif/smoke_test.cpp
git mv code/verif/team_api_tests.cpp        code/verif/team_api_tests.cpp
git mv code/verif/depth_tests.cpp           code/verif/depth_tests.cpp
git mv code/verif/fps_tests.cpp             code/verif/fps_tests.cpp
git mv code/verif/t0_full_tests.cpp         code/verif/t0_full_tests.cpp
git mv code/verif/t1_full_tests.cpp         code/verif/t1_full_tests.cpp
git mv code/verif/t2_full_tests.cpp         code/verif/t2_full_tests.cpp
git mv code/verif/t3_full_tests.cpp         code/verif/t3_full_tests.cpp
git mv code/verif/t4_full_tests.cpp         code/verif/t4_full_tests.cpp
git mv code/verif/evasion_full_tests.cpp    code/verif/evasion_full_tests.cpp
git mv code/verif/features_full_tests.cpp   code/verif/features_full_tests.cpp
git mv code/verif/ops_full_tests.cpp        code/verif/ops_full_tests.cpp
git mv code/verif/structural_full_tests.cpp code/verif/structural_full_tests.cpp
git mv code/verif/shared_full_tests.cpp     code/verif/shared_full_tests.cpp
git mv code/verif/tools_full_tests.cpp      code/verif/tools_full_tests.cpp

# ── Phase 5a: behavioral_decoupler flatten ─────────────────
mkdir -p code/crosscutting/features/behavioral_decoupler
git mv code/crosscutting/features/include/features/behavioral_decoupler.hpp \
       code/crosscutting/features/behavioral_decoupler/behavioral_decoupler.hpp
git mv code/crosscutting/features/src/behavioral_decoupler.cpp \
       code/crosscutting/features/behavioral_decoupler/behavioral_decoupler.cpp
rm -rf code/crosscutting/features/include code/crosscutting/features/src

# ── Phase 5b: adaptive_strategy flatten ────────────────────
mkdir -p code/crosscutting/ops/adaptive_strategy
git mv code/crosscutting/ops/red_strategy/include/ops/red/adaptive_strategy.hpp \
       code/crosscutting/ops/adaptive_strategy/adaptive_strategy.hpp
git mv code/crosscutting/ops/red_strategy/src/adaptive_strategy.cpp \
       code/crosscutting/ops/adaptive_strategy/adaptive_strategy.cpp
rm -rf code/crosscutting/ops/red_strategy

echo "git mv complete. Apply CMake/include/doc rewrites next (Phases 2–6 content edits)."
```

### Appendix B — Post-`git mv` content rewrite checklist

```text
[ ] CMakeLists.txt: T0..T4 short paths
[ ] CMakeLists.txt: AC_STRATEGY_PAIRS long→short
[ ] CMakeLists.txt: cs2_radar proto→demos
[ ] CMakeLists.txt: test sources → verif/
[ ] CMakeLists.txt: wire behavioral_decoupler.cpp + adaptive_strategy.cpp
[ ] verif/*_tests.cpp: #include tiers/tN/...
[ ] composition_radar_loop: #include tiers/t0/strategies/...
[ ] behavioral_decoupler.cpp / adaptive_strategy.cpp: local #include
[ ] Active docs + README path strings
[ ] INDEX.md + ARCHITECTURE.md + HISTORY.md
[ ] Optional: pair.cpp for gaming_chair + skin_changer + registry
[ ] cmake configure + build + ctest
```

### Appendix C — Path substitution one-liners (after git mv)

```bash
# From repo root — review diff before commit
cd path/to/periscope/radar

# CMake + sources (exclude archive snapshot + build)
rg -l 't0|t1|t2|t3|t4|demos/cs2_radar|shared/lab/src/(smoke|team_api|depth|fps|t[0-4]_full|evasion_full|features_full|ops_full|structural_full|shared_full|tools_full)' \
  code docs README.md \
  --glob '!code/build/**' \
  --glob '!docs/archive/2026-07-21-pre-comprehensive/**'

# Example bulk replace (run only after Phase 2 git mv)
perl -pi -e '
  s|tiers/t0|tiers/t0|g;
  s|tiers/t1|tiers/t1|g;
  s|tiers/t2|tiers/t2|g;
  s|tiers/t3|tiers/t3|g;
  s|tiers/t4|tiers/t4|g;
  s|demos/cs2_radar|demos/cs2_radar|g;
' code/CMakeLists.txt

perl -pi -e '
  s|shared/lab/src/smoke_test\.cpp|verif/smoke_test.cpp|;
  s|shared/lab/src/team_api_tests\.cpp|verif/team_api_tests.cpp|;
  s|shared/lab/src/depth_tests\.cpp|verif/depth_tests.cpp|;
  s|shared/lab/src/fps_tests\.cpp|verif/fps_tests.cpp|;
  s|shared/lab/src/t0_full_tests\.cpp|verif/t0_full_tests.cpp|;
  s|shared/lab/src/t1_full_tests\.cpp|verif/t1_full_tests.cpp|;
  s|shared/lab/src/t2_full_tests\.cpp|verif/t2_full_tests.cpp|;
  s|shared/lab/src/t3_full_tests\.cpp|verif/t3_full_tests.cpp|;
  s|shared/lab/src/t4_full_tests\.cpp|verif/t4_full_tests.cpp|;
  s|shared/lab/src/evasion_full_tests\.cpp|verif/evasion_full_tests.cpp|;
  s|shared/lab/src/features_full_tests\.cpp|verif/features_full_tests.cpp|;
  s|shared/lab/src/ops_full_tests\.cpp|verif/ops_full_tests.cpp|;
  s|shared/lab/src/structural_full_tests\.cpp|verif/structural_full_tests.cpp|;
  s|shared/lab/src/shared_full_tests\.cpp|verif/shared_full_tests.cpp|;
  s|shared/lab/src/tools_full_tests\.cpp|verif/tools_full_tests.cpp|;
' code/CMakeLists.txt
```

---

## Risk notes

| Risk | Mitigation |
|------|------------|
| Missed hard-coded long path | Phase 7 `rg` gate; fail if any remain outside archive |
| Include path break for `"tN/red/…"` | Never move files under `*/include/tN/` |
| cs2_radar include dir wrong | Update both `add_executable` sources **and** `target_include_directories` |
| Orphan wire breaks symbols | Link as private sources first; registry later |
| Registry collisions | Do not renumber this pass; only add 73/74 if free |

**End of plan.** Execute Phase 0 → 7; stop on red ctest.
