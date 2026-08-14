# User study protocol (offline outline research)

Simulation filters and contrast formulas are early indicators only. Testing with gamers
who have the target vision disabilities gives better data. This document defines the
offline study package, rating scales, and aggregation path. All inputs and outputs are
local files; nothing is uploaded.

Related: [ACCESSIBILITY.md](ACCESSIBILITY.md). Mask metrics: [EVALUATION.md](EVALUATION.md).

## Goals

Check whether configurable dual-contrast (and optional pattern) outlines are:

1. Useful for spotting players vs unaided viewing
2. Acceptable on clutter, comfort, and error-confusion
3. Configurable enough that participants would enable a preferred treatment

Does not claim WCAG conformance for CS2, does not evaluate aim assistance, and does not
invent universal acceptance thresholds without a pilot corpus.

## Ethical and operational boundaries

- Use only footage you are entitled to process (personal demos, licensed recordings).
- Obtain informed consent appropriate to your institution or informal pilot context.
- Store participant IDs as opaque local tokens (`P001`, …); avoid committing PII.
- Prefer short segments (about 10-30 seconds) with known scene difficulty tags.
- Provide an immediate-off path during any live preview (`q` / Escape, Ctrl+C, duration
 limits). Pack rendering itself is unattended batch only.
- Do not pulse or flash outlines; renderer patterns are static geometry only.

## Protocol measures

After each clip × condition stimulus, collect the following ratings. Likert items
use integers 1-5. Anchors are suggestions for facilitator scripts; keep wording
consistent within a pilot.

| Field | Type | Low anchor (1) | High anchor (5) |
|---|---|---|---|
| `usefulness` | Likert 1-5 | Not useful for spotting players | Very useful for spotting players |
| `clutter` | Likert 1-5 | No extra clutter | Overwhelming visual clutter |
| `comfort` | Likert 1-5 | Uncomfortable / fatiguing | Comfortable for the full clip |
| `small_player_visibility` | Likert 1-5 | Small/distant players still hard to see | Small/distant players clearly easier |
| `error_confusion` | Likert 1-5 | Errors not confusing / not noticed | Errors confusing or felt unsafe |
| `would_enable` | boolean | - | Participant would enable this treatment in their own play (true/false) |

Optional free-text `notes` capture preferences for width, color, fill, pattern, and
whether a non-visual cue would also help (see accessibility design).

These fields map to the initial evaluation list in [ACCESSIBILITY.md](ACCESSIBILITY.md):
detection usefulness versus clutter; comfort under motion/smoke/detail; small-player
visibility; outline error confusion; and whether participants would enable the effect.

## Study package schema (v1)

Example: [examples/study.v1.json](examples/study.v1.json).

Root object keys:

| Key | Required | Meaning |
|---|---|---|
| `schema_version` | yes | Must be `1` |
| `study_id` | yes | Single path segment (pack id) |
| `title` / `description` | no | Human-readable metadata |
| `clips` | yes | Non-empty array of clip objects |
| `conditions` | yes | Non-empty array of condition objects |
| `inference` | if any outline | Shared model settings for `kind=outline` |

### Clip object

| Key | Required | Meaning |
|---|---|---|
| `clip_id` | yes | Path segment; unique within the package |
| `input` | yes | Local video path (relative to the study JSON parent or absolute) |
| `label` / `notes` | no | Facilitator text |
| `tags` | no | Array of non-empty strings (`smoke`, `distance`, …) |

### Condition object

| Key | Required | Meaning |
|---|---|---|
| `condition_id` | yes | Path segment; unique within the package |
| `kind` | yes | `baseline` (no overlay) or `outline` |
| `label` / `description` | no | Shown in facilitator materials |
| `outline_preset` | outline | Named preset (`high-visibility`, …) |
| `prefs` | outline | Local outline preferences JSON path |
| style overrides | outline | `inner_color`, `outer_color`, widths, `fill_opacity`, `scale_with_frame`, `stroke_pattern`, `dash_period_px` |

For `kind=outline`, at least one of `outline_preset`, `prefs`, or a style override is
required. Style resolution precedence matches the outline CLI:
preset → prefs file → field overrides. Contrast ≥3:1 and width geometry still apply.
`kind=baseline` should omit outline style fields.

### Inference object

Required when any condition is `outline`:

| Key | Required | Default |
|---|---|---|
| `model` | yes | - |
| `manifest` | yes | - |
| `backend` | no | `ultralytics-onnx` |
| `class_names` | no | model default filter |
| `confidence` | no | `0.45` |
| `image_size` | no | `640` |
| `device` | no | `cpu` |
| `max_frames` | no | `18000` |
| `max_seconds` | no | unset |

Unknown keys anywhere in the package fail closed.

## Stimulus pack layout

```bash
uv run cs2-vision study-render \
 --study path/to/study.json \
 --output-directory artifacts/study-pack \
 --overwrite
```

Produces:

```text
artifacts/study-pack/
├── pack-manifest.v1.json
├── ratings.template.jsonl
└── stimuli/
    ├── smoke-mid-01__baseline.mp4
    ├── smoke-mid-01__high-visibility.mp4
    └── …
```

- baseline jobs copy the source clip (no model load).
- outline jobs run the same synchronous file-only outline path as `cs2-vision outline`.
- Stimulus stem is `{clip_id}__{condition_id}`.
- `--validate-only` expands the clip × condition plan and writes the pack manifest
 without decoding video or loading ONNX (useful for CI and dry runs).

## Rating JSONL schema (v1)

One JSON object per line (UTF-8). Required keys:

```json
{
 "schema_version": 1,
 "participant_id": "P001",
 "session_id": "S001",
 "clip_id": "smoke-mid-01",
 "condition_id": "high-visibility",
 "stimulus_id": "smoke-mid-01__high-visibility",
 "presented_order": 0,
 "ratings": {
 "usefulness": 4,
 "clutter": 2,
 "comfort": 4,
 "small_player_visibility": 3,
 "error_confusion": 2,
 "would_enable": true
 },
 "notes": "optional"
}
```

| Key | Required | Constraint |
|---|---|---|
| `schema_version` | yes | `1` |
| `participant_id` | yes | Non-empty string |
| `session_id` | yes | Non-empty string |
| `clip_id` / `condition_id` | yes | Path segments |
| `ratings` | yes | Exact protocol fields; no extras |
| `stimulus_id` | no | Usually `{clip_id}__{condition_id}` |
| `presented_order` | no | Non-negative integer |
| `notes` | no | Free text |

## Session procedure (facilitator checklist)

1. Consent and setup - Confirm entitled footage, consent, and that data stay local.
2. Calibration - Show one practice baseline + one outline clip not in the scored set.
3. Counterbalance - Randomize or Latin-square condition order per participant;
 record `presented_order` in the ratings JSONL.
4. Per stimulus - Play the pack MP4; allow one replay; then collect the six ratings
 immediately (paper → later JSONL entry, or direct local form → JSONL).
5. Exit questions - Preferred preset/pattern; disable/adjust needs; non-visual cue
 interest; open comments.
6. Immediate-off - If using interactive preview instead of pack MP4s, document that
 `q`/Escape ends display and that duration limits are available on the outline CLI.

Recommended pilot size is small (order of SeeingVR-scale single-digit to low-teen
participants with the target disabilities) with qualitative notes prioritized over
overfitted thresholds.

## Aggregation

```bash
uv run cs2-vision study-aggregate \
 --ratings artifacts/study-pack/ratings.jsonl \
 --output artifacts/study-pack/aggregate.v1.json
```

The summary reports rating counts, participant/session counts, overall mean Likert
fields, overall `would_enable` rate, and the same means stratified by `condition_id`.
It does not invent significance tests or pass/fail cutoffs.

## Library entry points

| Symbol | Role |
|---|---|
| `load_study_package` / `parse_study_package` | Schema validation |
| `plan_study_render` / `render_study_pack` | Clip × condition pack |
| `parse_rating` / `load_ratings_jsonl` | Rating validation |
| `aggregate_ratings` / `write_aggregate_json` | Offline summary |

## Out of scope (this release)

- Live capture or in-game overlay studies
- Networked survey platforms or remote upload
- Automatic IRB workflows
- Claiming product readiness from a single pilot
