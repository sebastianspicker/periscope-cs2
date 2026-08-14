---
title: CS2 YOLO11n-seg Training
emoji: 🎯
colorFrom: green
colorTo: blue
sdk: gradio
sdk_version: 5.0.0
app_file: app.py
pinned: true
short_description: Fine-tune YOLO11n-seg on CS2 player segmentation data
tags: [cs2, yolo, segmentation, training]
---

# CS2 YOLO11n-seg Training Space

Gradio UI for fine-tuning YOLO11n-seg on Counter-Strike 2 player segmentation
data and exporting FP32 and FP16 ONNX models plus a model manifest.

## Files to copy into a Hugging Face Space

Standalone Space deploys need all four of these next to each other:

| File | Source in this repo | Role |
|------|---------------------|------|
| `app.py` | `training/space/app.py` | Gradio UI |
| `requirements.txt` | `training/space/requirements.txt` | Space pip deps |
| `README.md` | `training/space/README.md` | Space card + docs (this file) |
| `cloud.py` | `training/cloud.py` (parent of `space/`) | Shared extract / train / ONNX / manifest |

Copy checklist:

1. `app.py`
2. `requirements.txt`
3. `README.md` (this file)
4. `cloud.py` from `../cloud.py` (that is, `src/cs2_vision_access/training/cloud.py`) into the Space root next to `app.py`

There is no second YOLO train pipeline inlined in `app.py`. If `cloud.py` is missing, the UI still loads but training fails with a clear error.

### Monorepo auto-load (no vendoring needed)

When you run from a full checkout, `app.py` tries helpers in this order:

1. Package import: `cs2_vision_access.training.cloud`
2. Sibling path: `training/cloud.py` (one directory up from `space/`)
3. Vendored path: `space/cloud.py` next to `app.py`

So in the monorepo you only need the `space/` directory contents for a local UI smoke test; package install or sibling `../cloud.py` supplies training.

## Deploy on Hugging Face Spaces

1. Create a new Space and copy the four files above into the Space root.
2. Hardware: choose a GPU runtime (T4 small is a good default). CPU is only suitable for launching the UI, not full training.
3. Prefer the standard Hugging Face GPU image so PyTorch and CUDA are already available. This Space's `requirements.txt` installs Ultralytics, Gradio, ONNX tooling, etc., but does not pin a CUDA-specific `torch` wheel (those differ by CUDA version and are brittle on Spaces).
4. Start the Space, open the app, upload a dataset zip, configure hyperparameters, and click Start Training.
5. Download:
   - FP32 ONNX for CPU-friendly inference
   - FP16 ONNX for GPU inference (when conversion succeeds)
   - Manifest JSON with SHA-256 and class metadata for local packaging

## Prepare the dataset zip (local)

From a checkout of this project (with the package installed):

```bash
# 1) Build a YOLO-style dataset (images/ + labels/) if you do not have one yet
python -m cs2_vision_access.training.prepare

# 2) Bundle for upload
python -m cs2_vision_access.training.bundle \
  --input data/cs2_train \
  --output data/cs2_train_bundle.zip
```

Useful options:

- `--max-frames N`: pack only the first N labeled frames (faster smoke runs)
- `--input` / `--output`: paths for the dataset directory and zip

Zip layout expected by the Space:

```text
dataset.yaml          # optional; generated if missing
images/*.jpg
labels/*.txt
```

Classes (fixed contract): `ct`, `ct_head`, `t`, `t_head`.

## Local smoke test

With Gradio installed (training deps optional for UI load):

```bash
cd src/cs2_vision_access/training/space
python app.py
```

The UI should open even without a GPU. Actual training requires Ultralytics,
ONNX tools, preferably CUDA, and loadable `cloud.py` helpers (package, sibling, or vendored).

## Hardware notes

| Setup              | Use case                                      |
|--------------------|-----------------------------------------------|
| HF T4 (or better)  | Real fine-tuning (~30-60 min for ~500 frames) |
| HF CPU             | UI / wiring checks only                       |
| Local GPU          | Same as Space; install CUDA torch yourself    |

## Related modules

- `cs2_vision_access.training.bundle`: dataset zip packaging
- `cs2_vision_access.training.cloud`: shared extract / train / ONNX / manifest
- `cs2_vision_access.training.prepare`: local dataset preparation
