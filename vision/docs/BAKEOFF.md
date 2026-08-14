# Offline backend bakeoff

Compare registered segmenter backends on the same local recorded video. File-only; no live capture.

The bakeoff emits latency and instance-count fields from `VideoRunSummary`. It does not declare a winner. You judge held-out CS2 boundary quality, small-player recall, p95 latency, and memory on your own hardware.

Typical pairs: `ultralytics-onnx` vs optional `rfdetr`, or vs `ultralytics-detect` / `cs2-sam` when you have the weights.

## CLI

Two-backend shorthand (pairs with `--model-a` / `--manifest-a` and `-b`):

```bash
uv run cs2-vision bakeoff \
  --input path/to/recording.mp4 \
  --backends ultralytics-onnx,rfdetr \
  --model-a models/yolo26n-cs2.onnx --manifest-a models/yolo26n-cs2.json \
  --model-b models/rfdetr-cs2.onnx --manifest-b models/rfdetr-cs2.json \
  --max-frames 300 \
  --output artifacts/bakeoff.json
```

Config file (N backends; relative paths resolve against the config directory):

```bash
uv run cs2-vision bakeoff --input path/to/recording.mp4 --config bakeoff.json
```

Example `bakeoff.json`:

```json
{
  "backends": [
    {
      "backend": "ultralytics-onnx",
      "model": "models/yolo.onnx",
      "manifest": "models/yolo.json"
    },
    {
      "backend": "rfdetr",
      "model": "models/rf.onnx",
      "manifest": "models/rf.json"
    }
  ],
  "max_frames": 300,
  "confidence": 0.45,
  "device": "cpu"
}
```

Optional RF-DETR install: `pip install 'cs2-vision-access[rfdetr]'` (or `uv sync` with the `rfdetr` extra).

Backend names match `create_segmenter` / `--segmenter-backend` ([ARCHITECTURE.md](ARCHITECTURE.md), [CONTRACTS.md](CONTRACTS.md)).

## Report schema (v1)

```json
{
  "schema_version": 1,
  "input": "path/to/recording.mp4",
  "max_frames": 300,
  "max_seconds": null,
  "winner": null,
  "notes": "Comparison only: ... No acceptance winner is declared.",
  "runs": [
    {
      "backend": "ultralytics-onnx",
      "model": "...",
      "manifest": "...",
      "frames_processed": 300,
      "instances_predicted": 120,
      "instances_outlined": 118,
      "inference_ms_p50": 12.5,
      "inference_ms_p95": 18.0,
      "pipeline_ms_p50": 15.0,
      "pipeline_ms_p95": 22.0,
      "frame_budget_misses": 4,
      "termination_reason": "max_frames",
      "eval": null
    }
  ]
}
```

- Numeric fields are finite (non-finite values fail closed before write).
- `winner` is always `null` by design.
- `eval` is reserved for optional mask-eval payloads when predictions are dumped separately (`eval-masks`); bakeoff does not invent metrics.

## Behavior

1. Resolve backend specs (CLI pairs or config). Empty backends list fails closed.
2. For each backend, construct a segmenter via `create_segmenter` and run `process_video` with no MP4 output (benchmark-style path).
3. Project `VideoRunSummary` comparison fields into each run row.
4. Write atomic sorted-key JSON to `--output` (default `artifacts/bakeoff.json`) and print the same payload to stdout.

## Module

`src/cs2_vision_access/bakeoff/` — config + runner orchestration; injectable `process_video_fn` / `segmenter_factory` for unit tests without GPU or weights.
