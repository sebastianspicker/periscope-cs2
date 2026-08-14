# Models directory

Optional local home for weights and manifests during development. Gitignored except
this README. Prefer `artifacts/` for downloads and train exports:

```bash
uv run cs2-vision download-model --list-models
uv run cs2-vision download-model yolo11n-seg --output-dir artifacts
```

Train outputs: `artifacts/runs/segment/`. train-auto: `artifacts/auto/<run_id>/models/`.

Do not commit large `.pt` / `.onnx` binaries.
