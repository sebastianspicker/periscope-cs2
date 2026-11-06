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
| X1 | cross | done | radar half + vision/lens halves: `NOTICE.md` records every adapted/referenced component with code/design/nothing distinction (radar upstreams in the radar commit; scrcpy/ncnn/Arian04/arXiv/webcam-clients in the cross commit); EdgeSAM registry fix in the vision commit | cross domain squash |
| X2 | cross | done | radar half: relay/offset self-tests in `radar-ci.yml` PR lane (radar commit); vision half: `vision-ci.yml` runs the tracked ledger tests on every PR (no gating ifs — verified 2026-11-06) | cross domain squash |
| X3 | cross | done | decisions live in `docs/ADAPTATION_LEDGER.md` do-not-adapt list; enforcement = NOTICE notes (X1) | done by design |
| X4 | cross | done | dated VACnet field-context note in `radar/SECURITY.md`, explicitly marked as one author's unverified observation supporting — not driving — the posture | cross domain squash |
| X5 | cross | done | license verdicts recorded per component in `NOTICE.md` + `vision/docs/MODEL-GOVERNANCE.md`; nothing code-copied without a verdict | cross domain squash |
| X6 | cross | done | root `SECURITY.md` scope table carries the Lens track (camera-only; actuation lab-only opt-in) | cross domain squash |
| X7 | cross | done | kmbox/Makcu/DMA-injector exclusion recorded in the ledger Lens addendum + LENS-OUTPUT-SUPPORT.md ("compatible hardware, never implemented") | cross domain squash |
| L1 | lens | done | `vision/docs/LENS-TOPOLOGY.md` (A/B/C decision record, latency budget, posture mapping, iOS deferral = L10) | lens domain squash |
| L2 | lens | done | `adapters/capture/phone.py` + `tests/test_ledger_l2_phone_source.py` (read-loop contract, scrcpy composition, backends); real bindings runtime-unverified (house style) | lens domain squash |
| L3 | lens | done | `application/lens/calibration.py` + `tests/test_ledger_l3_calibration.py` (exact/keystone/noise solves, drift monitor) | lens domain squash |
| L4 | lens | done | `application/lens/preflight.py` + `tests/test_ledger_l4_preflight.py` (banding/moiré/focus/coverage, advice strings) | lens domain squash |
| L5 | lens | sources done | `vision/lens-android/` (Kotlin: TargetsEncoder byte-mirror, HID probe, TransportHub, MainActivity seams) + README with honest gaps + benchmark template; no Android CI lane (recorded) | lens domain squash |
| L6 | lens | done | `application/lens/transport.py` + tests in `test_ledger_lens_transport.py` (round-trip all types, version rejection, unknown-type skip, truncation, oversize, length mismatch; u16→u32 length fix surfaced by tests) | lens domain squash |
| L7 | lens | done | `application/lens/latency.py` (RTT + outlier rejection, alpha-beta predictor, measured-vs-assumed breakdown) + tests | lens domain squash |
| L8 | lens | pure done | `application/lens/output.py` (path ladder, delta shaper, report encoder) + `docs/LENS-OUTPUT-SUPPORT.md`; configfs/agent runtime wiring documented as lab work | lens domain squash |
| L9 | lens | done | `application/lens/policy.py` (FOV gate, sticky lock, capped smoothed deltas, cooldown) + determinism tests; pure, no actuation | lens domain squash |
| L10 | lens | done | deferral note in `LENS-TOPOLOGY.md` | lens domain squash |

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
- 2026-11-06 (lens): lens Python modules live inside the vision package
  (`application/lens/`, `adapters/capture/phone.py`) rather than a new
  top-level subtree — camera-only perception inherits the vision posture,
  AGPL license and CI gates; the Android companion is a source drop under
  `vision/lens-android/` with no CI lane (recorded in its README).
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
