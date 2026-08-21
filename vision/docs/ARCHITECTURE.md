# Architecture

Package root: `src/cs2_vision_access/` under the Periscope monorepo path `vision/`. Entry points: `cs2-vision` console script (`cli:main`) and `python -m cs2_vision_access`. Sister monorepo track: `radar/` (memory-path lab; not used by this package).

Status: **pre-alpha** (0.1.x). Live, GUI, train-auto, bakeoff, eval, study, and labeling paths are in the tree. Models and acceptance thresholds are not product-validated.

Safety boundary: no game process handles, no memory read/write, no synthetic input APIs in this package.

## Runtime paths

```text
Offline file:
  video input → segmenter.predict → renderer → optional cue log / output video

Live:
  capture (screen | device | file) → inference pipeline → temporal filter (optional)
  → renderer → display / overlay backend / alpha sink
```

## Package map

```text
cs2_vision_access/
├── __init__.py              # __version__
├── __main__.py
├── safety.py                # external-only constraints
├── predictions.py           # InstanceMask and related types
├── model_manifest.py        # SHA-256 manifests for ONNX
├── dataset_split.py         # re-export of dataset.split public API
├── bakeoff/                 # sequential backend comparison
├── capture/                 # capture manager, overlay backends
├── cli/                     # parser and handlers
├── config/                  # settings models; package data JSON/YAML
├── cues/                    # offline cue log writer
├── dataset/                 # validate, audit, import, sessions, split
├── evaluation/              # masks, negatives, temporal, comfort
├── frames/                  # extract frames from video
├── gui/                     # desktop tkinter dashboard (cs2-vision gui)
├── inference/               # live pipeline, temporal, CUDA helpers
├── labeling/                # boxes-to-masks drafts, promote/review
├── prefs/                   # local outline preferences JSON
├── renderer/                # dual-stroke outlines, roles, supervision bridge
├── segmenters/              # create_segmenter factory and backends
├── study/                   # study pack render and rating aggregate
├── training/                # train, train-auto, cloud, prepare, remote, notebooks
└── video/                   # offline process_video loop
```

## Training subsystems

| Module | Role |
|--------|------|
| `training/local.py` | Session-split local train + ONNX export |
| `training/cloud/` | Flat-dir train helpers used by notebooks/Space |
| `training/auto/` | Config stages: ingest through report (`train-auto`) |
| `training/prepare_lib/` | Vombit+EdgeSAM prepare API |
| `training/bootstrap_labels.py` | COCO person silhouette bootstrap |
| `training/remote_autonomous/` | Notebook iterative loop |
| `training/self_train/` | One self-train pseudo-label pass |
| `training/contracts.py` | Class maps, layouts, train profiles, yaml writer |

## Segmenter factory

`segmenters.create_segmenter` picks a backend from CLI/config. Live, outline, benchmark, and bakeoff share `InstanceMask` predictions.

| Backend | Notes |
|---------|--------|
| `ultralytics-onnx` | Default. YOLO-seg ONNX via Ultralytics/ORT |
| `ultralytics-detect` | Box rectangles only (bootstrap / detect-as-outline) |
| `rfdetr` | Optional; install `cs2-vision-access[rfdetr]` |
| `cs2-sam` | Vombit detect + EdgeSAM masks (teacher / hybrid path) |
| `yolov10`, `nanodet` | Detect-only ONNX Runtime backends |

See [BAKEOFF.md](BAKEOFF.md) for offline multi-backend timing on one video.

## Temporal filter

Optional suppress-only stability filter (default off). Unstable one-frame blips stay hidden until a detection matches for N consecutive frames. Outline uses `--temporal-suppress`; live uses `--temporal-enabled`. Live also has optional hold-last flags (`--temporal-hold`, `--temporal-max-dropout`); hold is off by default and is not the accessibility default. Details: [ACCESSIBILITY.md](ACCESSIBILITY.md), [LIVE_MODE.md](LIVE_MODE.md).

## Desktop GUI

`gui/` implements `cs2-vision gui`:

- `GuiSettings` — Capture / Model / Style / Overlay-Output tab values
- `GuiPipelineController` — worker thread around `run_live_pipeline`; Start/Stop and action buttons
- `GuiApp` — tkinter window, preview, diagnostics, log, Save/Load config

The controller reuses the CLI engine: `on_frame` for the live preview, shared `LiveControlState` plus a `terminate_event` so the Run tab can start/stop and mirror hotkey actions (presets, width, mode, fill, pause, diagnostics) on every platform.

Overlay backends expose position APIs so the overlay can sit at any x/y or auto-align to the capture region / monitor.

## Role catalog (renderer)

Optional class-keyed multi-signifier treatments (`--role-config`): dual-stroke colors, `stroke_pattern`, and shape markers. Color-only multi-class catalogs fail closed. Example: [examples/role-catalog.v1.json](examples/role-catalog.v1.json). See [ACCESSIBILITY.md](ACCESSIBILITY.md).

## Data layouts

- Flat bootstrap: `images/` + `labels/` (+ `dataset.yaml`), often train=val for research only.
- Session split: `images/{train,val,test}/` + matching labels; whole sessions per split via `assemble-dataset`.

## Artifacts

Local outputs go under `artifacts/` (gitignored). Prefer manifests next to ONNX files. Auto-train runs use `artifacts/auto/<run_id>/` when `paths.work_root` is default.

## Related docs

| Doc | Topic |
|-----|--------|
| [CLI.md](CLI.md) | Command index |
| [LIVE_MODE.md](LIVE_MODE.md) | Live + overlay |
| [TRAIN_AUTO.md](TRAIN_AUTO.md) | Multi-stage train |
| [DATASET.md](DATASET.md) | Labels, assemble, train |
| [EVALUATION.md](EVALUATION.md) | Mask / temporal / comfort metrics |
| [CONTRACTS.md](CONTRACTS.md) | Segmenter / InstanceMask contracts |
| [CUDA.md](CUDA.md) | GPU extras |

Historical ledgers and design notes: [archive/](archive/) (not active product docs). Completed LoC split ledger: [archive/MONOLITH_LEDGER.md](archive/MONOLITH_LEDGER.md).
