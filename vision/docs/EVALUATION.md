# Evaluation metrics

Offline mask metrics: ground-truth YOLO polygons vs predicted pixel-space polygons.
The pure metric path does not load ONNX or need a GPU. Build a predictions cache with
`export-predictions`.

No universal cutoffs — pick thresholds after measuring a source-separated pilot on your
own footage. CLI index: [CLI.md](CLI.md).

## What is covered

| Capability | Status |
|---|---|
| Instance precision / recall and mean matched IoU | Implemented at a configurable IoU threshold. Full COCO-style mask mAP50-95 is not computed. |
| Boundary F1 | Implemented with configurable dilation radius (default 2 px). |
| Recall by visible area | Area bins (tiny / small / medium / large by pixel area). Distance is not estimated from 2D labels alone. |
| False positives per minute on no-player footage | `eval-negatives`. FP count on expected-empty frames, rate via `--fps`. |
| Results by map, side/skin, resolution, graphics, smoke, flash, blur, occlusion | Out of scope until structured per-frame/session condition provenance exists (session.json has rights/frame_policy only; no map/side/smoke schema to join). |
| Frame-to-frame contour instability | `eval-temporal`. Mean centroid displacement, mean IoU drop, presence flicker rate. |
| Comfort proxies (clutter / local stroke contrast) | `eval-comfort`. Raster clutter fraction and fraction of edge samples with outer-stroke vs local background contrast of at least 3:1. Proxies only, not user ratings. |
| Inference / end-to-end latency | Covered by `cs2-vision benchmark`, not `eval-masks`. |
| Predicted vs outlined counts, degenerate/stale, termination | Covered by video run summaries, not this module. |
| GPU / game FPS impact | Out of scope for file-only offline eval. |
| Low-vision user evaluation | [USER_STUDY.md](USER_STUDY.md) protocol plus `study-render` / ratings JSONL. |

## YOLO ground truth

Labels follow Ultralytics instance segmentation:

```text
class_id x1 y1 x2 y2 ... xn yn
```

Coordinates are normalized to `[0, 1]`. Empty label files are valid negatives.
`load_yolo_polygons` converts them to absolute pixel polygons using the image width and
height supplied with each prediction entry.

Dataset layout for `eval-masks`:

```text
<dataset-root>/
└── labels/
    └── <split>/
        ├── frame_a.txt
        └── frame_b.txt
```

Images under `images/{split}/` are not required for pure polygon metrics when image
dimensions are present in the predictions cache (or as payload defaults).

## Evaluation universe

Metrics cover the union of:

1. every stem listed under `predictions.images`, and
2. every regular `labels/{split}/.txt` stem.

| Situation | Treatment |
|---|---|
| Stem in predictions, label present | Compare preds vs GT. |
| Stem in predictions, label missing | Empty GT → all preds are false positives. |
| Stem in labels only (omitted from predictions) | Empty preds → all GT instances are false negatives. Requires `default_width` / `default_height` on the predictions payload (or an explicit empty `images` entry with size). |

`image_id` and `split` must be single path segments (no `/`, `\`, or `..`). Joined
label paths are resolved and rejected if they escape the labels split directory.

## Predictions cache schema (v1)

```json
{
 "schema_version": 1,
 "default_width": 64,
 "default_height": 64,
 "images": {
 "<stem>": {
 "width": 64,
 "height": 64,
 "predictions": [
 {
 "class_id": 0,
 "confidence": 0.91,
 "polygon": [[x, y], [x, y], [x, y]]
 }
 ]
 }
 }
}
```

- `stem` matches `labels/{split}/{stem}.txt` and must be a single path segment.
- Polygons are absolute pixel coordinates in the source frame and must have positive
 shoelace area and positive raster foreground count at the frame size
 (collinear, zero-area, and sub-pixel empty-raster instances are rejected).
- Confidence is optional (default `1.0`) and is recorded for forward compatibility;
 matching currently uses geometry only.
- `default_width` / `default_height` are required when any label stem lacks a
 predictions entry (so omitted stems still denormalize YOLO coordinates).

## Metrics

### Mask IoU and greedy matching

1. Rasterize each polygon onto a boolean mask of shape `(height, width)` using the
 pure-numpy fill (authoritative path; OpenCV is not used for eval metrics).
2. Compute IoU between same-class ground-truth and prediction pairs.
3. Match greedily by descending IoU, one-to-one, keeping pairs with IoU > 0 and
 IoU ≥ `--iou-threshold` (default `0.5`). Empty-empty and fully disjoint pairs
 produce IoU 0.0 even when the threshold is `0`.

Empty-empty rasterization (union zero) yields IoU `0.0`. True positives are matched
pairs. Unmatched predictions are false positives; unmatched ground truth are false
negatives. When both denominators are zero (no GT and no predictions), rates are
reported as `0.0` so JSON values remain finite.

### Boundary F1

For each matched pair:

1. Extract the 4-connected boundary of each raster mask.
2. Dilate both boundaries by `--boundary-dilation-px` (default `2`) with pure-numpy
 square dilation.
3. Precision = fraction of prediction boundary pixels that hit the dilated GT boundary.
4. Recall = fraction of GT boundary pixels that hit the dilated prediction boundary.
5. Boundary F1 is the harmonic mean.

Both-empty boundaries return 0.0 (aligned with empty-empty mask IoU). The run
reports `mean_boundary_f1` over matched pairs only.

### Recall by visible-area bins

Default bins use raster foreground pixel count of each GT instance (consistent
with IoU), not shoelace vector area:

| Name | Area range (px) |
|---|---|
| `tiny` | `[0, 1024)` |
| `small` | `[1024, 4096)` |
| `medium` | `[4096, 16384)` |
| `large` | `[16384, ∞)` |

Each bin reports ground-truth count, true positives, and recall. Empty bins report
`recall: 0.0` with zero counts.

### False positives per minute (`eval-negatives`)

Counts predicted instances only on frames that are expected empty:

1. `predictions.images[<stem>].expected_zero: true`, and/or
2. regular `labels/{split}/<stem>.txt` files with no instance rows (empty labels).

```text
false_positives_per_minute = false_positive_count / (frame_count / fps) * 60
```

Empty selection errors closed (no frames selected). Zero frames after a successful
selection path is not produced by the file loader; the pure function
`evaluate_negatives(())` reports finite `0.0` rates for unit tests.

Selection label in the result JSON:

| `selection` | Meaning |
|---|---|
| `expected_zero` | Only `expected_zero` flags. |
| `empty_labels` | Only empty YOLO label files (requires `--dataset-root`). |
| `expected_zero_and_empty_labels` | Union of both. |

When pairing with a dataset, predictions still supply the instance lists; label-only
empty stems need `default_width` / `default_height` (or an explicit empty images
entry) the same way as `eval-masks`.

### Temporal contour instability (`eval-temporal`)

Offline sequence of predicted polygons. Frames are sorted by `frame_index`. For each
consecutive pair:

1. Greedy same-class IoU match (same matcher as `eval-masks`).
2. For each match: centroid L2 displacement (px) and IoU drop `1 - IoU`.
3. Unmatched endpoints contribute to presence flicker.

Aggregate fields:

| Field | Definition |
|---|---|
| `mean_centroid_displacement_px` | Mean L2 shift over matched pairs (`0.0` if none). |
| `mean_iou_drop` | Mean of `1 - IoU` over matched pairs (`0.0` if none). |
| `presence_flicker_rate` | `(unmatched_prev + unmatched_next) / (count_prev + count_next)` summed over consecutive pairs; `0.0` when no instances. |

Sequence schema (v1 object or bare list of frames):

```json
{
 "schema_version": 1,
 "frames": [
 {
 "frame_index": 0,
 "width": 64,
 "height": 64,
 "predictions": [
 {
 "class_id": 0,
 "confidence": 0.9,
 "polygon": [[x, y], [x, y], [x, y]]
 }
 ]
 }
 ]
}
```

### Comfort proxies (`eval-comfort`)

Non-subjective stand-ins for clutter and stroke legibility. Not a substitute for
[USER_STUDY.md](USER_STUDY.md) ratings.

| Field | Definition |
|---|---|
| `clutter_fraction` | Union of rasterized prediction polygons / `(height * width)`. |
| `local_stroke_contrast_ge_3_fraction` | Fraction of edge samples where WCAG contrast between `--outer-color` luminance and local background (sampled outward from the polygon) is ≥ `contrast_threshold` (default `3.0`). |
| `contrast_sample_count` | Number of valid edge samples (off-frame samples skipped). |

Empty predictions → finite `0.0` fractions. Inner color is recorded for provenance;
the local sample currently compares the outer stroke to the background (dual-stroke
engineering target).

## CLI

### `eval-masks`

```bash
uv run cs2-vision eval-masks \
  --predictions path/to/predictions.v1.json \
  --dataset-root path/to/yolo-dataset \
  --split test \
  --output artifacts/eval-run.v1.json
```

| Flag | Meaning |
|---|---|
| `--predictions` | Schema v1 predictions JSON (required). |
| `--dataset-root` | YOLO root containing `labels/{split}/` (required). |
| `--split` | Label split name (default `test`). |
| `--iou-threshold` | Match threshold in `[0, 1]` (default `0.5`). |
| `--boundary-dilation-px` | Boundary F1 dilation radius (default `2`). |
| `--output` | Optional path to write the same schema-versioned JSON. |

Example fixture output shape: [examples/eval-run.v1.json](examples/eval-run.v1.json).

### `eval-negatives`

```bash
uv run cs2-vision eval-negatives \
  --predictions path/to/predictions.v1.json \
  --fps 30 \
  --dataset-root path/to/dataset \
  --split test \
  --output artifacts/eval-negatives.v1.json
```

| Flag | Meaning |
|---|---|
| `--predictions` | Schema v1 predictions JSON (required). |
| `--fps` | Positive frame rate for minute conversion (required). |
| `--dataset-root` | Optional YOLO root; empty `labels/{split}` files select frames. |
| `--split` | Label split when dataset is set (default `test`). |
| `--output` | Optional schema-versioned JSON path. |

Use `expected_zero: true` on prediction entries when no empty-label split is available.

### `eval-temporal`

```bash
uv run cs2-vision eval-temporal \
  --sequence path/to/sequence.v1.json \
  --iou-threshold 0.5 \
  --output artifacts/eval-temporal.v1.json
```

| Flag | Meaning |
|---|---|
| `--sequence` | Schema v1 sequence JSON (required). |
| `--iou-threshold` | Frame-to-frame match threshold (default `0.5`). |
| `--output` | Optional schema-versioned JSON path. |

### `eval-comfort`

```bash
uv run cs2-vision eval-comfort \
  --frame path/to/frame.png \
  --predictions path/to/predictions.v1.json \
  --outer-color "#101010" \
  --inner-color "#F6FF00" \
  --output artifacts/eval-comfort.v1.json
```

| Flag | Meaning |
|---|---|
| `--frame` | Source PNG/JPEG (required). |
| `--predictions` | Predictions list or single-stem images map (required). |
| `--outer-color` | Dark stroke `#RRGGBB` (default `#101010`). |
| `--inner-color` | Bright stroke `#RRGGBB` (default `#F6FF00`). |
| `--output` | Optional schema-versioned JSON path. |

Stdout is always the evaluation object (plus `"output"` when a file was written).

## Library entry points

| Function | Role |
|---|---|
| `load_yolo_polygons` | Parse one YOLO label file → pixel polygons. |
| `mask_iou` | Raster IoU of two polygons. |
| `match_instances` | Greedy same-class IoU matching. |
| `boundary_f1` | Dilated boundary F1. |
| `recall_by_area_bin` | Stratified GT recall. |
| `evaluate_predictions` | Aggregate mask metrics over `FrameEvaluation` rows. |
| `evaluate_masks_from_files` | CLI path: predictions JSON + dataset labels. |
| `evaluate_negatives` / `evaluate_negatives_from_files` | FP-per-minute on expected-empty frames. |
| `evaluate_temporal` / `evaluate_temporal_from_files` | Contour instability over a sequence. |
| `evaluate_comfort` / `evaluate_comfort_from_files` | Clutter + local stroke contrast proxies. |
| `write_metrics_json` | Generic schema-versioned JSON writer. |

All public result dataclasses are frozen. Result `.as_dict()` methods emit
`schema_version: 1` and finite numeric fields (non-finite values would be serialized as
`null`, but normal runs produce finite rates). Import metrics from
`cs2_vision_access.evaluation` (package modules such as `masks`, `geometry`,
`temporal`, `negatives`, `comfort`, and `raster`).

## What this does not claim

- No model is “good enough” because fixture F1 is high.
- No universal precision/recall, FP/min, flicker, or clutter cutoffs.
- Comfort proxies are not accessibility conformance claims.
- No substitute for source-separated CS2 holdout footage, user studies, or full video
 run summaries.
