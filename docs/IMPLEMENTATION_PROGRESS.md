# Implementation progress ledger — ADAPTATION_LEDGER execution

Companion to [`docs/ADAPTATION_LEDGER.md`](ADAPTATION_LEDGER.md). One row per
ledger item, updated as work lands. Merge discipline per the session goal:
each domain (Radar R, Vision V, Cross X, Lens L) is implemented on its own
branch, squashed to one commit, and locally merged into `main`.

**Final state (2026-11-06): all 38 items done.** Domain commits on `main`:

| Domain | Commit | Items |
|---|---|---|
| Radar | `0739f47` | R1–R12 |
| Vision | `e4ddc0a` | V1–V9 |
| Lens | `73bfd15` | L1–L10 |
| Cross | `e71d408` | X1–X7 |

A final restructure commit moves the Lens track out of `vision/` into the
top-level `lens/` subtree (structure: lens, radar, vision).

Nothing has been pushed (standing decision; local merges only). Two
proofreading commits follow the domain commits (`ef185d4`, `57713a1` plus
the follow-up corrections); they carry no domain content.

| Item | Track | Status | Evidence | Commit/domain |
|---|---|---|---|---|
| R1 | radar | done | `radar/src/domain/radar/snapshot.{hpp,cpp}`, `tests/radar_snapshot_test.cpp` (radar lane 10/10 CTest incl. stack_posture) | radar domain squash |
| R2 | radar | done | `radar/src/domain/radar/sampling.hpp`, `tests/radar_sampling_test.cpp` (7 branch groups) | radar domain squash |
| R3 | radar | done | `radar/src/domain/radar/snapshot_ring.hpp`, `tests/radar_ring_test.cpp` (threaded lap test) | radar domain squash |
| R4 | radar | done | `radar/src/domain/radar/projection.{hpp,cpp}`, `tests/radar_geometry_test.cpp` | radar domain squash |
| R5 | radar | core done | `radar/src/application/radar/webradar_service.{hpp,cpp}`, `tests/radar_webradar_test.cpp`; real socket adapter deferred to lab adapter window (recorded in webradar_service.hpp) | radar domain squash |
| R6 | radar | done | `radar/relay/{relay_core,relay_server}.py` + loopback self-test (11 checks, real sockets) + README; Caddy deployment recipe in README | radar domain squash |
| R7 | radar | done | `radar/src/domain/radar/intervals.hpp` (`adaptive_publish_hz`), `tests/radar_geometry_test.cpp` | radar domain squash |
| R8 | radar | done | `radar/scripts/resolve_dumper_ref.py` (self-test 12 groups) + `.github/workflows/radar-offset-sync.yml` (actionlint clean, drift check verified against live upstream exit 0) | radar domain squash |
| R9 | radar | done | `radar/src/domain/radar/offset_overrides.{hpp,cpp}`, `tests/radar_offsets_test.cpp` | radar domain squash |
| R10 | radar | policies done | `radar/src/domain/radar/scatter_batch.{hpp,cpp}` + `tests/radar_scatter_test.cpp`; live-adapter wiring deliberately deferred by audit posture — rules recorded in `radar/docs/DMA-SCATTER-POLICY.md` | radar domain squash |
| R11 | radar | portable done | `radar/src/domain/radar/los_bvh.{hpp,cpp}` + `tests/radar_los_test.cpp` (brute-force parity); `.tri` format `tri_loader.{hpp,cpp}` + round-trip test; game-asset extraction requires the game client (documented) | radar domain squash |
| R12 | radar | done | `radar/src/domain/radar/mini_radar.{hpp,cpp}`, `tests/radar_geometry_test.cpp` | radar domain squash |
| V1 | vision | done | `adapters/capture/sinks/shared.py` (`ProtocolVersionError`, `negotiate_protocol`, `header_to_json`), `tests/test_ledger_regressions.py` | vision domain squash |
| V2 | vision | done | `application/live/recording.py` + `tests/test_recording.py`; wired end-to-end via `--record` (V6 slice) | vision domain squash |
| V3 | vision | done | `application/live/performance.py` (advisory profiles, override precedence, bounded `LatencyWindow`), `tests/test_ledger_v3_performance.py`; present-latency capture wiring is backend-specific and documented as advisory | vision domain squash |
| V4 | vision | done | bbox prefilter in `workflows/evaluation/geometry.py::match_instances` + `tests/test_ledger_v4_prefilter.py` (parity over random scenes, skip-proof, edge cases) | vision domain squash |
| V5 | vision | doc done | `vision/docs/OVERLAY_COMPATIBILITY.md` (dated matrix, posture note); hide-from-capture flag stays in the excluded presenter area by decision | vision domain squash |
| V6 | vision | done | `--debug-viz`/`--record` CLI flags (smoke-verified in `live --help`), deterministic `renderer/debug.py`, `FrameRecorder` protocol + per-frame hook in `live_pipeline/frame.py`; `tests/test_ledger_v6_debug.py` | vision domain squash |
| V7 | vision | harness done | `application/live/cv_validate.py` + `tests/test_ledger_v7_validator.py` + `vision/docs/CV-VALIDATOR-SPIKE.md`; measured report pending real recordings (recorded in doc checklist) | vision domain squash |
| V8 | vision | done | `tests/test_ledger_regressions.py` (A3/V1 incident set, tracked, PR-run); A4/A5 guards already in `test_live_core.py` (house) | vision domain squash |
| V9 | vision | done | `tests/test_model_governance.py` + `vision/docs/MODEL-GOVERNANCE.md`; registry license fix (EdgeSAM NTU-S-Lab-1.0, was wrongly "MIT") + NOTICE update | vision domain squash |
| X1 | cross | done | radar half + vision/lens halves: `NOTICE.md` records every adapted/referenced component with code/design/nothing distinction (radar upstreams in the radar commit; lens refs later moved to `lens/NOTICE.md`; EdgeSAM registry fix in the vision commit) | cross domain squash + restructure |
| X2 | cross | done | radar half: relay/offset self-tests in `radar-ci.yml` PR lane (radar commit); vision half: `vision-ci.yml` runs the tracked ledger tests on every PR (no gating ifs — verified 2026-11-06) | cross domain squash |
| X3 | cross | done | decisions live in `docs/ADAPTATION_LEDGER.md` do-not-adapt list; enforcement = NOTICE notes (X1) | done by design |
| X4 | cross | done | dated VACnet field-context note in `radar/SECURITY.md`, explicitly marked as one author's unverified observation supporting — not driving — the posture | cross domain squash |
| X5 | cross | done | license verdicts recorded per component in `NOTICE.md` / `lens/NOTICE.md` + `vision/docs/MODEL-GOVERNANCE.md`; nothing code-copied without a verdict | cross domain squash + restructure |
| X6 | cross | done | root `SECURITY.md` scope table carries the Lens track (camera-only; actuation lab-only opt-in) | cross domain squash |
| X7 | cross | done | kmbox/Makcu/DMA-injector exclusion recorded in the ledger Lens addendum + `lens/docs/OUTPUT-SUPPORT.md` ("compatible hardware, never implemented") | cross domain squash |
| L1 | lens | done | `lens/docs/TOPOLOGY.md` (A/B/C decision record, latency budget, posture mapping, iOS deferral = L10) | lens domain squash |
| L2 | lens | done | `lens/src/periscope_lens/phone.py` + `lens/tests/test_phone_source.py` (contract, scrcpy composition, backends); real bindings runtime-unverified (house style) | lens domain squash |
| L3 | lens | done | `lens/src/periscope_lens/calibration.py` + `lens/tests/test_calibration.py` (exact/keystone/noise solves, drift monitor) | lens domain squash |
| L4 | lens | done | `lens/src/periscope_lens/preflight.py` + `lens/tests/test_preflight.py` (banding/moiré/focus/coverage, advice strings) | lens domain squash |
| L5 | lens | sources done | `lens/android/` (Kotlin: TargetsEncoder byte-mirror, HID probe, TransportHub, transport lifecycle) + README with honest gaps + benchmark template; no Android CI lane (recorded) | lens domain squash |
| L6 | lens | done | `lens/src/periscope_lens/transport.py` + `lens/tests/test_transport.py` (round-trip all types, version rejection, unknown-type skip, truncation, oversize, length mismatch; u16→u32 fix surfaced by tests) | lens domain squash |
| L7 | lens | done | `lens/src/periscope_lens/latency.py` (RTT + outlier rejection, alpha-beta predictor, measured-vs-assumed breakdown) + tests | lens domain squash |
| L8 | lens | pure done | `lens/src/periscope_lens/output.py` (path ladder, delta shaper, report encoder) + `lens/docs/OUTPUT-SUPPORT.md`; configfs/agent runtime wiring documented as lab work | lens domain squash |
| L9 | lens | done | `lens/src/periscope_lens/policy.py` (FOV gate, sticky lock, capped smoothed deltas, cooldown) + determinism tests; pure, no actuation | lens domain squash |
| L10 | lens | done | deferral note in `lens/docs/TOPOLOGY.md` | lens domain squash |

## Done: anti-cheat split (S-series)

Owner request 2026-11-06: `radar/` becomes a clean slate for the game-facing
radar (real examples, adapters, live reading); the lab moves to a new
`anti-cheat/` subtree. Full plan, dependency evidence, phases, acceptance
and rollback: [`docs/ANTI_CHEAT_SPLIT_LEDGER.md`](ANTI_CHEAT_SPLIT_LEDGER.md).

**Final state (2026-11-06): all 13 steps done.** One commit per phase, each
verified green before landing:

| Phase | Commit | Content |
|---|---|---|
| 0 | `c85290a` | S1–S3 seam: `LR_ANTI_CHEAT_DIR`, options split, `LrHelpers.cmake`, library cmake + `build_keys.hpp.in` git-mv'd to `anti-cheat/cmake/` |
| 1 | `7713581` | S4a–S4i extraction: 1422-file `git mv` of lab sources/docs/demos/scripts/stack_posture test; cmake re-rooted on `AC_SOURCE_DIR`; checker paths re-pointed |
| 2 | `4186c38` | S5–S7 re-composition: lab AppTargets/TestTargets + full `ac_lab` root project; radar = `periscope_radar` with real/hybrid apps + 9 tests |
| 3 | `eb7cf6d` | S8–S10 docs/CI/checker: anti-cheat README/NOTICE/SECURITY/LICENSE/CONTRIBUTING + docs INDEX; radar product README/NOTICE; `anti-cheat-ci.yml` + `verify.py anti-cheat-sim`; full `check_anti_cheat` |
| 4 | this commit | S11 wording sweep (both trees), S12 ledger close-out, S13 final checks |

Acceptance evidence (all run locally on macOS, portable, real flags OFF):
`verify.py all` green — architecture ✓; anti-cheat-sim CTest 1/1 +
strategy_lab 187/0; radar-sim CTest 9/9 (was 10/10, `stack_posture_test`
moved) + 187/0; vision + lens lanes ✓. actionlint clean on both workflows.
Stale-path grep (`radar/src/simulation`, `radar/scenarios`) over tracked
live files: 0 actionable hits (remaining hits are historical ledgers and an
intentional checker comment). R1–R12 evidence paths in the tables above are
unchanged — radar modules stayed in `radar/` as planned. Real-capability
Windows dispatch job not re-run locally (no push; it consumes both trees by
unchanged design) — noted as pending for the next push.

| Step | Phase | Status | Evidence / notes |
|---|---|---|---|
| S1 dependency seam | 0 | done | `LR_ANTI_CHEAT_DIR` cache var + include order in `radar/CMakeLists.txt`; commit `c85290a` |
| S2 options split | 0 | done | lab Options (`LR_BUILD_*`, `LAB_ALLOW_*`, seeds, `LR_BUILD_ID`, build_keys) in `anti-cheat/cmake/Options.cmake`; real flags + hardening stay in `radar/cmake/Options.cmake` |
| S3 shared helpers + cmake moves | 0 | done | `DomainTargets/SimulationTargets/ScenarioTargets/ScenarioSources/TeamSources/build_keys.hpp.in` git-mv'd; `LrHelpers.cmake` owns `lr_build_options`/`lr_library`/`lr_exe`/platform macros; stub `anti-cheat/CMakeLists.txt` configures |
| S4a–S4i lab extraction | 1 | done | lab sources/docs/demos/scripts/stack_posture test git-mv'd to `anti-cheat/`; lab-side cmake paths re-rooted on `AC_SOURCE_DIR` (radar block in DomainTargets guarded by `if(RADAR_SOURCE_DIR)`); architecture checker paths re-pointed; radar build green, CTest 10/10, strategy_lab 187/0 |
| S5 AppTargets split | 2 | done | `anti-cheat/cmake/AppTargets.cmake` (lab apps + `ac_pipeline` + strip loop); radar AppTargets keeps real/hybrid set; `strategy_lab` dual-registered (anti-cheat first, radar `if(NOT TARGET)` guard) so the radar-sim lane keeps its 187-pair run |
| S6 TestTargets split | 2 | done | radar registers 9 `radar_*_test` (9/9); `anti-cheat/tests/CMakeLists.txt` registers `stack_posture_test` (1/1) via test-only `LR_RADAR_DIR` seam for the adapters include root |
| S7 root CMakeLists split | 2 | done | `anti-cheat/CMakeLists.txt` = full `project(ac_lab)` (libs + lab apps + tests, no RealTargets); `radar/CMakeLists.txt` = `project(periscope_radar)` (was `ac_sim`); radar still includes anti-cheat library cmake |
| S8 README/NOTICE/SECURITY split | 3 | done | `anti-cheat/{README,CONTRIBUTING,SECURITY,LICENSE,NOTICE}.md` + `anti-cheat/docs/INDEX.md`; `radar/README.md` product-only; `radar/NOTICE.md` carved from root NOTICE; root four-tree maps (anti-cheat first); link check 0 dangling |
| S9 CI + verify lanes | 3 | done | `.github/workflows/anti-cheat-ci.yml` (portable lane, pinned actions); radar-ci paths gain `anti-cheat/**`; `verify.py` gains `anti-cheat-sim` + `all` order; actionlint clean |
| S10 architecture checker | 3 | done | `check_anti_cheat`: no-real + no-`#if 0` over lab src+scenarios, lab domain purity, required files, no-GLOB, no real flags in lab Options, pipeline disambiguation; radar domain purity extended with `ac/`; negative probes verified |
| S11 wording sweep | 4 | done | both trees: lab docs re-pathed to `anti-cheat/build/dev` + `../../radar/…` pointers (incl. tier LESSON sweep, verify_features scripts); radar ARCHITECTURE/BUILD-AND-TEST/CONTRIBUTING product-only with lab pointers; link checks 0 dangling; `verify_features.sh` runs 35/0 |
| S12 progress ledger | 4 | done | this block finalized |
| S13 final checks | 4 | done | `verify.py all` green (all five lanes); actionlint ✓; stale-path grep clean; clean tree |

Recorded deviations from the plan text (justified, none behaviour-changing):
- `build_keys.hpp.in` moved to `anti-cheat/cmake/` in Phase 0 (ledger said it
  stays in `radar/cmake/` until S5–S7): it is the lab's per-build simulation
  key header, generated by anti-cheat's Options and consumed by `ac_sim`.
- `ScenarioSources.cmake` moved along with `ScenarioTargets.cmake` (the
  ledger's S3 list implied it; `ScenarioTargets` includes it directly).
- `LR_BUILD_ID` option declared once, in `anti-cheat/cmake/Options.cmake`
  (it feeds the lab-owned build_keys watermark), not in radar's Options.
- `anti-cheat/tests/stack_posture_test.cpp` includes `real/cs2/posture.hpp`
  (header-only, portable) from radar's adapters root: S4i moves the test per
  the ledger, so S10's "no `real/` includes" scan is scoped to
  `anti-cheat/src` + `anti-cheat/scenarios`, with the tests directory
  documented as the deliberate exception (it audits the real stack's
  posture from the lab side).

## Working notes

- 2026-11-05: goal opened. Repo at `main` = `2d3eed3` (ahead of origin by 5,
  unpublished by standing decision). Branch plan:
  `feat/ledger-radar` → squash → main; then `feat/ledger-vision`,
  `feat/ledger-cross`, `feat/ledger-lens` (actual order: vision, lens, cross — cross last so its NOTICE entries could cover the lens refs).
- House verification gates: `python3 scripts/verify.py all` (architecture +
  radar portable build/CTest 187 strategy pairs + vision ruff/mypy/pytest).
  Every domain merge must leave this green.
- Scope judgement calls are recorded inline in the ledger rows and at the
  bottom of this file as they happen.

## Judgement calls made so far

- 2026-11-05/06 (radar): R5's real socket adapter and R10's live DMA wiring
  are deferred with recorded reasons (audit posture excludes unreviewed
  real-adapter work); their portable, policy/test halves are fully
  implemented. R11's game-asset extraction requires the game client; the
  `.tri` format + loader + BVH consumer are the deliverable half.
- 2026-11-06 (radar): R6 relay implemented first-party in Python stdlib
  (asyncio + hand-rolled RFC6455) instead of introducing a Go toolchain —
  the repo has no Go lane, and the pure policy core + real-socket loopback
  self-test give stronger local evidence than an unbuilt Go module would.
- 2026-11-06 (radar): two bugs found and fixed by the new tests themselves
  (serializer missing string quotes; transposed Möller–Trumbore dot product)
  — the acceptance tests did their job.
- 2026-11-06 (vision): the V9 registry hygiene gate immediately caught a real
  upstream-license error — EdgeSAM was recorded as "MIT" but is NTU S-Lab
  License 1.0 (non-commercial); fixed in the registry, NOTICE.md, and the
  governance doc, with the license-notice coupling now test-enforced.
- 2026-11-06 (vision): V2's CLI wiring is deliberately deferred to the V6
  slice (debug/visibility flags land together, one CLI surface change); the
  recorder/replay core and its acceptance tests are complete.
- 2026-11-06 (lens, superseded): the lens modules were first implemented
  inside the vision package and **moved out the same day** at the owner's
  request: the repository structure is now `lens/`, `radar/`, `vision/`.
  The package `periscope_lens` is self-contained (numpy only, enforced by
  the architecture check) with its own pyproject, lock, lane and CI; the
  Android companion lives in `lens/android/` (source drop, no CI lane,
  recorded in its README). X5 verdicts now live in `lens/NOTICE.md` (restructure commit).
- 2026-11-06 (lens): the L6 header length field grew u16→u32 because the
  512 KiB frame cap is inexpressible in 16 bits — caught by the protocol
  tests before any peer implementation existed.
- 2026-11-06 (lens): EdgeSAM license lesson generalized — every lens-side
  third-party reference (scrcpy, ncnn, Arian04, MIRAGE) keeps its X5 verdict
  in the cross-domain commit; nothing was code-copied, only reimplemented.
- 2026-11-06 (deep run): module-by-module logic review found (all fixed
  with regression tests): expired relay sessions were only pruned at the
  next mint, not at use; a producer's room claim leaked if the WS handshake
  failed after the claim; malformed fixed-size transport payloads raised
  bare struct.error instead of ProtocolError (and encode had no u8 box-count
  guard); the RTT tracker rejected a *sustained* latency regime change
  forever (two consecutive outliers now adopt it); a NaN in the HID shaper
  wedged reports() in an infinite loop; a backwards clock stalled the
  webradar interval gate; phone-source close leaked the capture handle; the
  Android activity never started its transport and the README overclaimed
  pipeline wiring. Three fuzzy test assertions were tightened to exact
  expectations.
- 2026-11-06 (proofread pass 2): the shared-ring MAGIC constant
  (`0x4356_3153`) puts the bytes "S1VC" on the wire while every doc calls it
  "CV1S" — the name is the big-endian spelling. The recorded wire contract
  guard wins: constant unchanged, quirk documented at the constant, and
  `header_to_json` derives its display text with the big-endian spelling so
  constant, mirror and docs agree on the name. Also fixed this pass: a
  latent UnboundLocalError in the L2 test helper, `tri_loader` now assembles
  little-endian explicitly (host-endianness independent), the relay producer
  limiter is per producer connection built from the room's own policy, and
  the debug-overlay import moved out of the hot path. Follow-up corrections
  to this pass itself: the snapshot serialization-bound comment said 292
  million years (correct: ≈4.9 billion), `_close_ws` also catches
  RuntimeError (transport closing), and the magic guard test now pins both
  spellings so the documented relationship cannot rot.
