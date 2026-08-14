# Generated artifacts

Local, gitignored outputs from training, export, benchmarks, bakeoff, study packs, and
renders. Nothing here ships with the repository (except this README).

## Layout

```text
artifacts/
├── README.md                         # this file (tracked)
├── runs/
│   └── segment/
│       └── <run-name>/               # Ultralytics train (default run-name: cs2-player)
│           ├── weights/
│           │   ├── best.pt
│           │   ├── best.onnx
│           │   └── best.model.json   # SHA-256 manifest for that ONNX
│           └── …                     # plots, args, results
├── auto/<run_id>/                    # train-auto work root (default paths.work_root)
├── yolo26n-seg.onnx                  # optional COCO-person plumbing baseline
├── yolo26n-seg.model.json
├── cs2-player-seg.onnx               # optional promoted CS2 player export
├── cs2-player-seg.model.json
├── *-outlined.mp4
├── bakeoff.json                      # bakeoff report default
└── *.benchmark.json
```

Defaults for `cs2-vision train`:

| Flag | Default |
|---|---|
| `--project-directory` | `artifacts/runs/segment` |
| `--run-name` | `cs2-player` |

After train, copy or symlink ONNX + manifest to flat names under `artifacts/` if you
prefer that over the Ultralytics `weights/` tree for `outline` / `live` / `benchmark`.

## COCO person vs CS2 player

| Path | Class | Purpose |
|---|---|---|
| COCO person baseline | `person` (COCO-80) | Plumbing smoke only. Not CS2-validated. |
| CS2 player train | `0=player` | Session-split fine-tune; ONNX + manifest under `runs/segment/<run-name>/`. |

See [docs/DATASET.md](../docs/DATASET.md), [docs/TRAIN_AUTO.md](../docs/TRAIN_AUTO.md),
and the top-level [README.md](../README.md).
