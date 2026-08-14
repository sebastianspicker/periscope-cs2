# Local datasets

Stage annotated sessions under `data/staging/<session_id>/` (flat image+label pairs,
or `images/` + `labels/` subtrees). Assemble by whole `session_id` only — not by
random-splitting adjacent frames:

```bash
uv run cs2-vision assemble-dataset \
  --staging-root data/staging \
  --output-root data/cs2_players \
  --train session-001,session-002 \
  --val session-003 \
  --test session-004
```

Expected training layout:

```text
data/cs2_players/
├── sessions.json          # written by assemble-dataset for audit-dataset
├── images/
│   ├── train/<session_id>/
│   ├── val/<session_id>/
│   └── test/<session_id>/   # optional
└── labels/
    ├── train/<session_id>/
    ├── val/<session_id>/
    └── test/<session_id>/
```

Each image needs a same-stem `.txt` label. An empty label file is a valid negative.

Full contract, box-import, draft review, and train commands: [docs/DATASET.md](../docs/DATASET.md).
