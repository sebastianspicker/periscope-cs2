# CV-as-validator research spike (ledger V7)

Status: protocol + harness implemented, measurements pending real
recordings. Nothing in this document establishes a result; it fixes the
question, the method, and the boundary conditions so the spike cannot drift
into an actuator.

## Research question

Can a second, deliberately cheap screen-space computer-vision pass *validate*
the primary Vision pipeline - catching occlusion mistakes, stale outlines,
and capture-path degradation - without ever generating input events?

The contrast that makes this research rather than engineering: deadlocked and
vesta solve occlusion from the memory side (BVH/LOS queries). This track has
no memory access by posture, so the only available cross-check is a second
look at the same pixels. The spike measures whether that second look agrees
with the first often enough to be useful as a *confidence signal*.

## Non-negotiable boundaries

1. Passive only. The validator computes agreement statistics. It never
   moves a cursor, synthesizes input, or writes to a game process. If the
   spike ever "graduates" into an actuator, that is a new project with its
   own posture review - not an extension of this one.
2. Cheap on purpose. The validator must cost meaningfully less than the
   primary pipeline (bbox-level matching, small model or classical CV), or it
   validates nothing in real time.
3. Measured, never assumed (decision 6 of the ledger): agreement
   thresholds are set from measured distributions on recorded sessions, not
   from intuition.

## Method

- Input: V2 session recordings (NDJSON, sanitized, absolute records).
  Both sides of the comparison must come from the *same recorded frames*, so
  the study is reproducible offline - no live game needed.
- Metric: per-frame greedy one-to-one bounding-box IoU matching between
  the reference instance set (primary pipeline) and the validator set
  (`application/live/cv_validate.py`, class-agnostic on purpose). Outputs:
  agreement rate, mean IoU of matches, missed/spurious counts per frame.
- Protocol:
  1. Export the primary pipeline's predictions for a recording as instance
     sets (the same shapes the study workflow already consumes).
  2. Run the candidate validator over the same frames (its own capture of
     the recording's frames, or a stored secondary detection pass).
  3. `ValidatorReport.record_frame(...)` per frame; `summary()` at the end.
  4. Report the summary *with the recording manifest* (device, resolution,
     scene composition) - an agreement number without its corpus is noise.
- Decision rule to be measured, then fixed: the spike report records the
  observed agreement distribution across recordings; only after that
  distribution exists does a threshold (e.g. "validator agreement below X on
  a stable scene indicates capture degradation") get written down, with the
  corpus that justifies it.

## What would falsify the idea

- Agreement that collapses exactly when the primary pipeline is *wrong*
  (the validator shares the primary's blind spots - same pixels, same
  failure).
- Validator cost within noise of the primary pipeline's inference cost.
- Agreement dominated by trivial frames (empty scenes) such that per-frame
  rates are meaningless without conditioning on non-empty frames - the
  harness already separates these via `reference_count == 0` handling.

## Deliverable checklist

- [x] Passive agreement harness with tests (`tests/test_ledger_v7_validator.py`).
- [x] This protocol document.
- [ ] Measured agreement report over ≥ 3 recorded sessions (pending real
      captures; runtime-unverified by house style until then).
- [ ] Fixed thresholds with their justifying corpus (blocked by the above).

## Provenance

arrozet/AImbot's pipeline shape (capture → YOLO → postprocess) read as a
*passive observer*; memory-side occlusion reasoning (deadlocked BVH, vesta
visibility checks) as the contrast; the 2026-10-09 VACnet field note in the
ledger (X4) as the argument for keeping the track passive.
