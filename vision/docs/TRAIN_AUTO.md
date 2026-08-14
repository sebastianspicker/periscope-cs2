# train-auto

Multi-stage training from a JSON or YAML config. CLI:

```bash
uv run cs2-vision train-auto --config <path> [--force] [--from-stage NAME]
# or
uv run python -m cs2_vision_access.training.auto --config <path>
```

Package layout and related entry points: [ARCHITECTURE.md](ARCHITECTURE.md). Dataset
contracts: [DATASET.md](DATASET.md).

## Modes

| Mode | Dataset expectation | Train backend |
|------|---------------------|---------------|
| `session_split` | `images/{train,val}` (+ labels); whole sessions | Local train path, or cloud via a flattened bundle when `train.backend=cloud` |
| `flat_cloud` | Flat `images/` + `labels/` (or zip) | Cloud helper when `train.backend=cloud` |

`session_split` with `train.backend=cloud` is accepted: the split tree is
flattened into a flat `images/` + `labels/` bundle under `{run_dir}/cloud/`
(`dataset/` + `dataset.zip`) before cloud training.

## Stage order

From `training/auto/state.py`:

```text
ingest → prepare_data → label → validate → train → export → self_train → eval → report
```

| Stage | Behavior |
|-------|----------|
| ingest | Record sources (zip, prebuilt roots, staging, videos, plan) |
| prepare_data | Extract zip, extract video frames, assemble session plan |
| label | Sparse-label teachers: `coco_person` (default), `edgesam`, or `none` |
| validate | Structure checks; class-id bounds; leaky-val policy for flat |
| train | Fine-tune; writes `progress/` under the run dir when possible |
| export | Promote ONNX/manifest; optional smoke inference; package zip (flat_cloud) |
| self_train | Optional pseudo-label + multi-iter retrain (default off) |
| eval | Optional `export-predictions` + mask metrics |
| report | `report.json`, `events.jsonl` |

Human gate (optional) sits before train when `human_gate.enabled` and `human_gate.block` are true (exit code 2). On block the run writes `progress/human_gate.json` (completed stages, artifacts, optional `uncertain_review` path) and the exit message includes those paths when present. Unattended multi-iter configs must keep the gate off.

Resume: `state.json` under `{work_root}/{run_id}/`. `--force` clears completions. `--from-stage` re-runs from a named stage and scrubs later artifacts.

## Label teachers

| `label.teacher` | Behavior |
|-----------------|----------|
| `coco_person` | YOLO-seg COCO person bootstrap |
| `edgesam` | Vombit+EdgeSAM via `prepare_lib` (optional download with `label.download_edgesam`) |
| `none` | No auto-label |

Example EdgeSAM config: `configs/train-auto.edgesam.example.json`.

## Self-train / unattended multi-iter

Optional `self_train` block. Top-level `autonomous: true` enables multi-iter self-train defaults (iterations default 3 when omitted), forces human_gate off, and does not force EdgeSAM (models required separately).

Loop after first train+export:

```text
(self_train → retrain) × N
```

Stop conditions: `stop_on_no_growth`, `max_plateau_iters`, conf schedule, optional teacher strategy (`prev_student` default).

Example: `configs/train-auto.unattended.example.json`.

## Example configs

| Path | Intent |
|------|--------|
| `configs/train-auto.example.json` | flat_cloud |
| `configs/train-auto.session-split.example.json` | prebuilt session tree |
| `configs/train-auto.edgesam.example.json` | EdgeSAM label |
| `configs/train-auto.self-train.example.json` | self-train enabled |
| `configs/train-auto.unattended.example.json` | autonomous multi-iter |

Edit `sources.*` and `train.device` for your machine. Default `paths.work_root` is `artifacts/auto`.

## Outputs

Under `{work_root}/{run_id}/` typically:

- `state.json`, `config.snapshot.json`, `report.json`, `events.jsonl`
- `models/` promoted ONNX + manifest
- `progress/` dataset analysis and train curves when available
- `package/` zip on flat_cloud export when configured

## Related modules

- Remote notebooks: `training/remote_autonomous.run_autonomous_loop` (separate from this stage machine)
- Manual prepare: `uv run python -m cs2_vision_access.training.prepare`
- Profiles and class maps: `training/contracts.py`
- CLI index: [CLI.md](CLI.md)
