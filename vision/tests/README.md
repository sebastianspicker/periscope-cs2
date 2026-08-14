# Tests

Pytest suite for `cs2_vision_access`. Config: `pyproject.toml` → `[tool.pytest.ini_options]`.

## Layout

```text
tests/
├── conftest.py           # markers and shared hooks
├── fixtures/             # checked-in fixtures (eval masks, etc.)
├── test_*.py             # unit and integration tests
└── README.md
```

## Run

```bash
uv run pytest tests/ -k "not gpu and not cuda"
# optional: skip live-heavy modules entirely
uv run pytest tests/ \
  --ignore=tests/test_live_capture.py \
  --ignore=tests/test_live_pipeline.py \
  --ignore=tests/test_live_cli.py \
  -k "not gpu and not cuda"
```

Repo-root CI (`.github/workflows/vision-ci.yml`, `working-directory: vision`) uses the same
filter on Ubuntu and Windows for Python 3.11–3.13. Public CI is CPU unit tests only; it does
not run real Ultralytics epochs or hardware overlay validation.

## Coverage by domain

| Area | Modules |
|------|---------|
| CLI | `test_cli_*.py`, `test_download_model.py` |
| Capture / live / GUI | `test_live_*.py` (often ignored in CI), `test_gui.py` |
| Renderer | `test_renderer_outline_*.py`, `test_renderer_roles.py`, `test_supervision_bridge.py` |
| Segmenters / inference | `test_segmenter.py`, `test_optimize.py`, `test_cuda_*.py` |
| Dataset / frames / labeling | `test_dataset*.py`, `test_frames.py`, `test_import_boxes.py`, `test_labeling_*.py` |
| Evaluation / study | `test_evaluation_*.py`, `test_export_predictions.py`, `test_study.py` |
| Training | `test_training*.py`, `test_train_auto_*.py`, `test_remote_autonomous_*.py`, `test_cloud_*.py` |
| Other | `test_cues.py`, `test_prefs.py`, `test_safety.py`, `test_model_manifest.py`, `test_bakeoff.py`, `test_video_*.py`, `test_temporal_policy.py` |

LoC size gate (post-split regression check, not product docs): `test_monolith_loc_gate.py`.

Historical shims (not collected): `archive/tests/obsolete-shims/`.

Fixtures under `tests/fixtures/` are versioned. Generated outputs, coverage reports, and
caches are gitignored (`vision/.gitignore` and monorepo ignore rules as applicable).
