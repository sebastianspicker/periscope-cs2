---
title: CS2 YOLO11n-seg Training
emoji: 🎯
colorFrom: green
colorTo: blue
sdk: gradio
sdk_version: 6.17.3
app_file: app.py
pinned: true
short_description: Fine-tune YOLO11n-seg on CS2 player segmentation data
tags: [cs2, yolo, segmentation, training]
---

# CS2 YOLO11n-seg Training Space

Gradio UI for fine-tuning YOLO11n-seg on Counter-Strike 2 player segmentation
data and exporting FP32 and FP16 ONNX models plus a model manifest.

## Deployment layout

Deploy the package layout rather than copying a partial standalone app. The
Space entry point imports security and training helpers from the package:

- `cs2_vision_access.training.space.app` and its `_gpu_security` and
  `_request_security` siblings
- the complete `cs2_vision_access.training.cloud` package
- shared training modules imported by that package
- this Space card and `requirements.txt`

Installing the project package or copying the complete `src/cs2_vision_access`
tree preserves that layout. A lone `app.py` or legacy `cloud.py` copy is not a
supported deployment artifact. There is no second YOLO train pipeline inlined
in `app.py`; missing helpers fail closed.

### Monorepo auto-load (no vendoring needed)

When you run from a full checkout, `app.py` tries helpers in this order:

1. Package import: `cs2_vision_access.training.cloud`
2. Sibling package: `training/cloud/` (one directory up from `space/`)
3. Vendored package: `space/cloud/` next to `app.py`

The full checkout supplies the package import directly. Do not publish only the
`space/` directory as a release artifact.

## Deploy on Hugging Face Spaces

1. Create a new Space and deploy the complete package layout above.
2. Hardware: choose a GPU runtime (T4 small is a good default). CPU is only suitable for launching the UI, not full training.
3. Prefer the standard Hugging Face GPU image so PyTorch and CUDA are already available. This Space's `requirements.txt` installs Ultralytics, Gradio, ONNX tooling, etc., but does not pin a CUDA-specific `torch` wheel (those differ by CUDA version and are brittle on Spaces).
4. Set Space secrets `CS2_SPACE_AUTH_USER` and `CS2_SPACE_AUTH_PASSWORD`. The app refuses to launch without both values, and uses them for Gradio basic authentication.
5. After startup, verify an unauthenticated request receives `401 Unauthorized` before allowing uploads.
6. Open the authenticated app, upload a dataset zip, configure hyperparameters, and click Start Training.
7. Download:
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
dataset.yaml          # ignored and replaced by the fixed local Space manifest
images/*.jpg
labels/*.txt
```

Classes (fixed contract): `ct`, `ct_head`, `t`, `t_head`.

## Local smoke test

With Gradio installed (training deps optional for UI load):

```bash
cd src/cs2_vision_access/workflows/training/space
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
