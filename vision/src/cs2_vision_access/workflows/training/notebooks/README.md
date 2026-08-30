# Cloud training notebooks

Colab and Kaggle notebooks call `cs2_vision_access.workflows.training.remote_autonomous.run_autonomous_loop` for iterative YOLO-seg training aimed at class `player` silhouettes.

| Notebook | Platform |
|----------|----------|
| `colab.ipynb` | Google Colab (GPU runtime recommended) |
| `kaggle.ipynb` | Kaggle (GPU + Internet) |

## Default data path

When no dataset zip is found, the loop can download a small slice of
[RekaAI/CS2-10k](https://huggingface.co/datasets/RekaAI/CS2-10k) (WebDataset tar shards).
License: CC BY-NC 4.0 (research / non-commercial). Record dataset license with any shared weights.

## Default knobs (generator)

| Setting | Default |
|---------|---------|
| `USE_CS2_10K` | True |
| Maps | mirage,dust2 |
| Max shards | 1 |
| Videos / frames per video | 20 / 10 |
| `ITERATIONS` | 4 |
| `EPOCHS_PER_ITER` | 0 (use profile `cloud_t4` epochs) |
| `CONF_SELF_TRAIN` | 0.45 |
| `CONF_SCHEDULE` | True |
| `USE_TEACHER_GATE` | True |
| `CS2_10K_HOLDOUT_FRAC` | 0.2 |
| `BOOTSTRAP` | True |
| `USE_EDGESAM` | False (opt-in Vombit+EdgeSAM auto-label) |
| `allow_leaky_val` | False (held-out val) |
| Profile | cloud_t4 |
| Resume | True |

## Pipeline

```text
optional zip OR CS2-10k frames
  → sparse-label bootstrap (COCO person, or EdgeSAM if USE_EDGESAM)
  → held-out train/val
  → (train → self-train) × N
  → package ONNX + manifest + progress/
```

## Package install in notebooks

1. `CS2_VISION_REPO` pip URL if set
2. Local clone under `/content` or `/kaggle`
3. Optional `CS2_VISION_GIT` clone when import still fails

## Outputs

Under the notebook `OUTPUT_DIR` (for example `/content/cs2_data`):

- `cs2-yolo11n-seg.onnx` (+ FP16 when conversion works)
- `cs2-yolo11n-seg.model.json`
- `cs2-yolo11n-seg-bundle.zip`
- `autonomous_report.json`
- `progress/` (report.md, plots, metrics)

Live after download:

```bash
cs2-vision live --model cs2-yolo11n-seg.onnx --manifest cs2-yolo11n-seg.model.json --class-name player
```

## Regenerate notebooks

From the repository root:

```bash
uv run python scripts/_gen_remote_notebooks.py
```

Edit the generator rather than hand-editing `.ipynb` when possible.

## Local alternative

For resumable multi-stage runs on a workstation, use `cs2-vision train-auto` and
[docs/TRAIN_AUTO.md](../../../../../docs/TRAIN_AUTO.md).
