# Dataset and training workflow

Session-split CS2 player polygons, staging, draft review, and `cs2-vision train`.
Auto-train stages: [TRAIN_AUTO.md](TRAIN_AUTO.md). CLI index: [CLI.md](CLI.md).

## Dataset contract

Canonical format: Ultralytics YOLO instance segmentation.

```text
<class-id> <x1> <y1> <x2> <y2> ... <xn> <yn>
```

Coordinates are normalized to `[0, 1]`. Each instance has at least three points. One
image has one same-stem `.txt` file; an empty file is a valid frame with no player.
Ultralytics documents this format at
[Instance Segmentation Datasets](https://docs.ultralytics.com/datasets/segment).

Expected layout:

```text
data/cs2_players/
├── images/
│   ├── train/
│   ├── val/
│   └── test/          # optional; image and label sides must both exist
└── labels/
    ├── train/
    ├── val/
    └── test/
```

Validate with:

```bash
uv run cs2-vision validate-dataset \
  --root data/cs2_players \
  --class-count 1
```

The validator checks:

- required train and validation directories;
- image-label pairing and orphan labels;
- symlinked roots, directories, images, and labels;
- contiguous numeric class range;
- an even coordinate count with at least three points;
- finite normalized coordinates;
- nonzero polygon area;
- empty negatives.

Audit structural integrity and train/val session leakage with:

```bash
uv run cs2-vision audit-dataset \
  --root data/cs2_players \
  --class-count 1 \
  --sessions data/cs2_players/sessions.json
```

`sessions.json` may be:

- an object mapping relative image paths, split-relative paths, or directory prefixes to
 `session_id` (prefer path/directory keys);
- a list of `{ "session_id": "...", "images": [...] }` objects;
- a single session record object (list-of-one form without the outer array).

Bare basename stems are applied only when the stem is unique across the whole dataset;
reuse of `0001.png` under train and val does not force a shared session. Keys that are
exactly `train`, `val`, or `test` are ignored as directory prefixes so nested session
folders keep working - bind a whole split with a path key such as `images/train` if needed.

When `--sessions` is omitted, `<root>/sessions.json` is used if present. A symlinked
default or explicit sessions path fails closed (`SYMLINKED_PATH`). The audit fails when
the same `session_id` appears in both train and val (and against an optional test split);
leakage messages include sample image paths. Pass `--check-decode` to attempt OpenCV
image decode when OpenCV is installed; if OpenCV is unavailable the audit stays valid but
sets `decode_skipped_reason` to `opencv_unavailable` in the JSON summary.

The validator alone does not find perceptual duplicates or verify polygon
self-intersection; keep those as separate dataset-quality gates.

## Label definition

Initial class map:

```yaml
0: player
```

Label pixels visibly belonging to a third-person player model.

Include:

- visible head, torso, arms, legs, and worn equipment;
- partially visible pixels up to the real occlusion boundary;
- both T and CT models without using team as a separate class.

## Source and split policy

Record source metadata for every image:

- source identifier and rights/consent record;
- demo or recording session;
- map and round/timestamp;
- team/agent if known from visible content;
- resolution, aspect ratio, graphics preset, and capture method;
- annotation author/tool/version;
- whether a SAM draft was used and who reviewed it.

Assign complete source sessions to train, validation, or test. Adjacent frames are nearly
duplicates; a random frame split measures memorization, not generalization.

## Assemble from staged sessions

Annotate under `data/staging/<session_id>/` (same-stem image+label pairs, or nested
`images/` and `labels/` trees). Then assign whole `session_id` values:

```bash
uv run cs2-vision assemble-dataset \
  --staging-root data/staging \
  --output-root data/cs2_players \
  --train session-001,session-002 \
  --val session-003 \
  --test session-004
```

Or pass a JSON plan:

```bash
uv run cs2-vision assemble-dataset \
  --staging-root data/staging \
  --output-root data/cs2_players \
  --plan configs/session-split-plan.json
```

```json
{"train": ["session-001", "session-002"], "val": ["session-003"], "test": ["session-004"]}
```

The command copies files into `images/{split}/{session_id}/` and matching labels,
writes `sessions.json` for `audit-dataset`, and refuses a session listed in more than
one split. Pass `--overwrite` only when replacing a previous assembly. Split is
session-based, not by random-split of individual frames.

Use perceptual hashing and manual contact sheets to find near-duplicate public clones.
Freeze the test set before hyperparameter tuning.

## Privacy and provenance

Frames may include Steam names, avatars, chat, voice indicators, notifications, and
unrelated desktop content.

For organizational research, document purpose, lawful basis, consent, source and
redistribution rights, retention, deletion, and access control.

## Frame sampling

Extract a bounded sample into an empty directory:

```bash
uv run cs2-vision extract-frames \
  --input data/local-demo.mp4 \
  --output-directory data/staging/session-001 \
  --every-n-frames 30 \
  --max-saved-frames 1000
```

Optional provenance flags:

```bash
uv run cs2-vision extract-frames \
  --input data/local-demo.mp4 \
  --output-directory data/staging/session-001 \
  --every-n-frames 30 \
  --max-saved-frames 1000 \
  --session-id session-001 \
  --notes "offline demo playback; entitled local recording"
```

On success, `extract-frames` writes a `session.json` provenance sidecar beside the PNG
frames. The output directory must be empty before extraction (including no prior
`session.json`) to prevent accidental overwrite.

`session.json` schema (version 1):

| Field | Description |
|---|---|
| `schema_version` | Integer `1` |
| `session_id` | Non-empty id; defaults to the output directory name |
| `source_stem` | Stem of the input video path |
| `rights` | Rights/consent object (status defaults to `"placeholder"` until filled; edit before redistribution) |
| `capture_notes` | Free-text operator notes (default empty string) |
| `frame_policy` | Sampling policy and counts from this extraction |

`rights` fields (all strings; fill placeholders before assembly/share):

- `status` - starts as `"placeholder"` until rights are recorded
- `source_identifier`
- `consent_record`
- `redistribution`
- `retention_notes`

`frame_policy` fields (positive/non-negative integers as appropriate):

- `every_n_frames`, `max_saved_frames` - requested sampling policy
- `decoded_frames`, `saved_frames` - actual counts for this run (`saved_frames` is capped
 at min(decoded, `max_saved_frames`))

Load/validate with `SessionProvenance.load` / `SessionProvenance.from_mapping` in
`cs2_vision_access.frames`. This staging sidecar is distinct from the dataset-level
`sessions.json` used by `audit-dataset`.

Cover:

- small/distant players;
- rapid camera motion and motion blur;
- smoke, flash, shadows, low light, and bright sky;
- crouch, jump, ladders, corners, doors, and partial bodies;
- all maps, teams, agents, common skins, aspect ratios, and graphics presets;
- player-free frames and visually similar negatives.

Start with a diverse pilot, plot the learning curve, and expand where held-out errors are
concentrated.

## Box-to-mask bootstrap

Public CS2 bounding-box datasets can reduce search effort, but boxes are not masks:

1. audit the dataset license, source, duplication, and split;
2. import local YOLO-det trees into staging (CLI below; no download automation);
3. convert boxes to draft polygons (CLI below) or pass each box to SAM 2;
4. have a human edit or reject every mask;
5. apply the visible-pixel policy consistently;
6. add negatives from independent footage;
7. preserve provenance and attribution.

### Import local box datasets into staging

Operator-supplied sources (download yourself, audit license first):

- [keremberke/csgo-object-detection](https://huggingface.co/datasets/keremberke/csgo-object-detection) (CC BY 4.0; boxes)
- RF100 / LibreYOLO CS:GO videogame YOLO exports (CC BY 4.0; boxes)
- Roboflow CS2 player detection exports (check each Universe license)

Layouts accepted by `import-box-dataset`:

1. Ultralytics YOLO - `images/{train,val[,test]}/` + `labels/{train,val[,test]}/` same-stem `.txt` boxes
2. Flat - `images/` + `labels/` side by side
3. Pairs (auto only) - one directory of same-stem image + `.txt` pairs

```bash
uv run cs2-vision import-box-dataset \
  --source-root /path/to/local/yolo-det \
  --output-staging data/staging \
  --session-id imported-keremberke \
  --split train \
  --layout auto \
  --max-images 500
```

Writes `data/staging/<session-id>/{images,labels}/` plus `session.json` provenance
(`rights.consent_record` notes `imported external box labels; audit license`).
Fail closed on missing pairs, symlinks, and empty sources. Pass `--overwrite` to
replace a non-empty session dir; optional `--link` hardlinks when the filesystem allows.

Then convert detection labels (`class_id x_c y_c w h`) into draft YOLO-seg polygons
with `boxes-to-masks`. Outputs are always `review_status: draft_pending` in
`draft_status.json` - drafts require human review before being treated as ground truth.

```bash
uv run cs2-vision boxes-to-masks \
  --images-dir data/staging/imported-keremberke/images \
  --labels-dir data/staging/imported-keremberke/labels \
  --output-labels-dir data/staging/imported-keremberke/labels_draft \
  --backend rectangle \
  --class-map ct=0,t=0,cthead=0,thead=0
```

Backends:

- `rectangle` (default) - four corners of each box;
- `ellipse` - ellipse inscribed in the box (≥16 points);
- `sam` - optional SAM prompt path (local checkpoint via `CS2_VISION_SAM_MODEL`;
 sam2 / Ultralytics SAM are not hard package dependencies).

Default class mapping sends every source class id to `0` (player). Pass `--overwrite`
only when replacing prior draft labels. Symlinked inputs/outputs fail closed.

SAM 2 code/checkpoints are Apache-2.0, but source-image rights remain independent:
[official SAM 2 repository](https://github.com/facebookresearch/sam2).

### Review draft masks (`review-drafts`)

SAM and geometry drafts require human promotion to ground truth. After
`boxes-to-masks`, every file starts as `review_status: draft_pending` (batch-level
and per-file in `draft_status.json`). A human must accept or reject each stem
before labels enter a GT layout.

`schema_version` remains `1`. Allowed `review_status` values:

| Value | Meaning |
|-------|---------|
| `draft_pending` | Bootstrap default; not reviewed |
| `accepted` | Human accepted the draft polygon label |
| `rejected` | Human rejected; blocked from promotion |
| `promoted` | Accepted draft was copied into a GT labels directory |

```bash
# Inspect status
uv run cs2-vision review-drafts list \
  --draft-status data/staging/boxes/labels_draft/draft_status.json

# Accept one stem, several stems, or every pending draft
uv run cs2-vision review-drafts accept \
  --draft-status data/staging/boxes/labels_draft/draft_status.json \
  --stem frame_a
uv run cs2-vision review-drafts accept \
  --draft-status data/staging/boxes/labels_draft/draft_status.json \
  --stems frame_a,frame_b
uv run cs2-vision review-drafts accept \
  --draft-status data/staging/boxes/labels_draft/draft_status.json \
  --all-pending

# Reject a stem
uv run cs2-vision review-drafts reject \
  --draft-status data/staging/boxes/labels_draft/draft_status.json \
  --stem frame_bad

# Promote accepted drafts into a GT labels directory (excludes pending/rejected)
uv run cs2-vision review-drafts promote \
  --draft-status data/staging/boxes/labels_draft/draft_status.json \
  --draft-labels-dir data/staging/boxes/labels_draft \
  --output-labels-dir data/cs2_players/labels/train \
  --only-accepted
```

`accept` / `reject` rewrite `draft_status.json` atomically. `promote` copies only
eligible `.txt` labels (mirroring nested relative paths) and sets those entries to
`promoted`. Pass `--only-accepted` to exclude already-promoted re-copies. Symlinked
status/label paths fail closed.

## Training

The dataset config is [configs/cs2-players.yaml](../configs/cs2-players.yaml)
(session-split product layout: `images/train` + `images/val`, `nc: 1`, `0=player`).
Class maps and layouts are SSOT in
`cs2_vision_access.training.contracts` (`PRODUCT_CLASSES`, `Layout.SESSION_SPLIT`).

Flat bootstrap datasets (prepare/batch) use the same-dir layout
(`train: images` / `val: images`) with Vombit multi-class labels - see
`training/dataset.yaml` and `VOMBIT_CLASSES`. That is distinct from the COCO
`person` baseline used only to exercise outline plumbing (see README “Try the
generic person-segmentation baseline”).

### Artifact layout

Training writes Ultralytics runs under `artifacts/` by default:

```text
artifacts/runs/segment/<run-name>/
├── weights/best.pt
├── weights/best.onnx          # ONNX export
└── weights/best.model.json    # SHA-256 manifest for that ONNX
```

Defaults: `--project-directory artifacts/runs/segment`, `--run-name cs2-player`. Promote
stable flat names (e.g. `artifacts/cs2-player-seg.onnx`) before long-lived `outline` /
`benchmark` runs if preferred. See [artifacts/README.md](../artifacts/README.md).

Multi-stage runs via `train-auto` write under `artifacts/auto/<run_id>/` by default
([TRAIN_AUTO.md](TRAIN_AUTO.md)).

### Full CS2 player fine-tune

```bash
uv run cs2-vision train \
  --dataset-yaml configs/cs2-players.yaml \
  --dataset-root data/cs2_players \
  --class 0=player \
  --base-model yolo26n-seg.pt \
  --base-model-origin https://docs.ultralytics.com/models/yolo26/ \
  --exported-model-license AGPL-3.0-only \
  --allow-model-download \
  --epochs 100 \
  --image-size 640 \
  --batch -1 \
  --device 0
```

### Few-epoch smoke (plumbing only)

`--smoke` forces a one-epoch, batch-1 run without changing the rest of the contract.
Explicit `--epochs` / `--batch` still win. Prefer `--device cpu` without a GPU. This is
not a CS2-validated model.

```bash
uv run cs2-vision train \
  --dataset-yaml configs/cs2-players.yaml \
  --dataset-root data/cs2_players \
  --class 0=player \
  --base-model yolo26n-seg.pt \
  --base-model-origin https://docs.ultralytics.com/models/yolo26/ \
  --exported-model-license AGPL-3.0-only \
  --allow-model-download \
  --smoke \
  --device cpu \
  --run-name cs2-player-smoke
```

CI and unit tests do not execute real Ultralytics training; they mock the backend and
assert the dataset YAML contract plus hyperparameter resolution only.

Ultralytics training code and produced weights are AGPL-3.0 by default according to its
published license guidance. A different framework/backend needs a separate
implementation and verification path.

## Validation and error analysis

Use mask metrics and visible-boundary metrics:

- mask mAP50-95;
- mask recall, with special attention to small visible regions;
- boundary F1 or boundary IoU;
- false positives per minute in no-player clips;
- temporal outline flicker;
- performance slices for map, skin, graphics, motion, smoke, flash, distance, and
 occlusion.

Review every persistent false positive and every high-impact miss. Feed those cases into
an active-learning queue while keeping the frozen test set untouched.
