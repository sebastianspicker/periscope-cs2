"""Kaggle notebook cell definitions."""

from __future__ import annotations

from .format import code, md
from .install import _kaggle_install_cell
from .shared import _PROGRESS_CELL, train_loop_cell_source


def build_kaggle_cells() -> list[dict]:
    return [
        md(
            """# CS2 player shape training (Kaggle)

Train YOLO-seg for CS2 player silhouettes (class `player`).

Settings: Accelerator → GPU, Internet → On, then Run All.

Free-tier path when the network is available: data → bootstrap shapes → train →
self-label → retrain for N iterations.

Default data: RekaAI/CS2-10k (auto-download 1 shard). Optional: Add Input zip.
Outputs under `/kaggle/working/cs2_data` (Output tab).

Validation defaults to held-out train/val (`allow_leaky_val=False`).
Profile `cloud_t4`, `resume=True`. Knobs: teacher gate, conf schedule,
CS2-10k holdout fraction, bootstrap. Optional leaky val via `ALLOW_LEAKY_VAL=1`.

Labels: default bootstrap is COCO person mapped to `player`. Set
`USE_EDGESAM=True` for Vombit+EdgeSAM teachers (downloads more ONNX assets;
more disk and time).
"""
        ),
        code(_kaggle_install_cell()),
        code(
            r"""# @title 2) Config + CS2-10k / zip discovery
import os
from pathlib import Path

# Target class: **player** silhouettes / shapes (PRODUCT_CLASSES {0: player})

DATASET_ZIP = os.environ.get(
    "CS2_DATASET_ZIP",
    "/kaggle/input/cs2-training-data/cs2_train_bundle.zip",
)

# --- shape-training defaults ---
ITERATIONS = 4  # @param {type:"slider", min:1, max:8, step:1}
EPOCHS_PER_ITER = 0  # @param {type:"integer"}  # 0 = profile cloud_t4 epochs
CONF_SELF_TRAIN = 0.45  # @param {type:"number"}
CONF_SCHEDULE = True  # @param {type:"boolean"}
USE_TEACHER_GATE = True  # @param {type:"boolean"}  # multi-iter teacher = prev best
CS2_10K_HOLDOUT_FRAC = 0.2  # @param {type:"number"}
USE_CS2_10K = True  # @param {type:"boolean"}
BOOTSTRAP = True  # @param {type:"boolean"}
USE_EDGESAM = False  # @param {type:"boolean"}  # opt-in Vombit+EdgeSAM auto-label
MIN_LABEL_RATIO = 0.05  # @param {type:"number"}

CS2_10K_MAPS = "mirage,dust2"  # @param {type:"string"}
CS2_10K_MAX_SHARDS = 1  # @param {type:"integer"}
CS2_10K_MAX_VIDEOS = 20  # @param {type:"integer"}
CS2_10K_FRAMES_PER_VIDEO = 10  # @param {type:"integer"}

BATCH = 16  # @param {type:"slider", min:2, max:32, step:2}
IMGSZ = 416  # @param {type:"integer"}
BASE_MODEL = "yolo11n-seg.pt"  # @param ["yolo11n-seg.pt", "yolo11s-seg.pt"]
OUTPUT_DIR = "/kaggle/working/cs2_data"  # @param {type:"string"}
# Held-out val default; set ALLOW_LEAKY_VAL=1 for flat train=val research only.
ALLOW_LEAKY_VAL = os.environ.get("ALLOW_LEAKY_VAL", "").strip().lower() in (
    "1", "true", "yes",
)  # @param {type:"boolean"}

# --- EdgeSAM auto-label (opt-in Vombit+EdgeSAM) ---
USE_EDGESAM = False  # @param {type:"boolean"}  # opt-in: Vombit+EdgeSAM auto-label
DOWNLOAD_EDGESAM = True  # @param {type:"boolean"}  # download teachers if missing
EDGESAM_ARTIFACTS = ""  # @param {type:"string"}  # empty = data_dir/edgesam_assets
EDGESAM_CONF = 0.4  # @param {type:"number"}
EDGESAM_COCO_FALLBACK = True  # @param {type:"boolean"}

zip_path = resolve_remote_dataset_zip(
    DATASET_ZIP if Path(DATASET_ZIP).is_file() else None,
    search_roots=["/kaggle/input", "/kaggle/working", "."],
)
if zip_path is None and not USE_CS2_10K:
    raise FileNotFoundError(
        "No dataset zip under /kaggle/input and USE_CS2_10K=False. "
        "Add Input zip or enable USE_CS2_10K (default)."
    )
print("Target class: player silhouettes / shapes")
print("Dataset zip:", zip_path if zip_path else "(none; RekaAI/CS2-10k)")
print(f"CS2-10k: {USE_CS2_10K} maps={CS2_10K_MAPS}")
print(f"  holdout_video_fraction={CS2_10K_HOLDOUT_FRAC}")
print(
    f"Loop: {ITERATIONS} iters | conf={CONF_SELF_TRAIN} schedule={CONF_SCHEDULE} "
    f"teacher={USE_TEACHER_GATE}"
)
print(f"Bootstrap={BOOTSTRAP} use_edgesam={USE_EDGESAM} min_label_ratio={MIN_LABEL_RATIO}")
print(f"Val: allow_leaky_val={ALLOW_LEAKY_VAL} (default False = held-out split)")
print(
    f"EdgeSAM: use={USE_EDGESAM} download={DOWNLOAD_EDGESAM} conf={EDGESAM_CONF} "
    f"coco_fallback={EDGESAM_COCO_FALLBACK} artifacts={EDGESAM_ARTIFACTS or '(default)'}"
)
if USE_EDGESAM:
    print(
        "USE_EDGESAM=True: multi-ONNX teacher download (Vombit+EdgeSAM); "
        "disk usage and runtime will increase."
    )
print("Profile: cloud_t4 | resume: True")
"""
        ),
        code(train_loop_cell_source()),
        code(_PROGRESS_CELL),
        code(
            r"""# @title 4) Outputs (Output tab + FileLink)
from pathlib import Path

out = Path(OUTPUT_DIR)
for p in sorted(out.glob("*")):
    if p.is_file():
        print(f"{p.name:40s} {p.stat().st_size/1e6:8.2f} MB")
try:
    from IPython.display import FileLink, display
    for name in (
        "cs2-yolo11n-seg.onnx",
        "cs2-yolo11n-seg-fp16.onnx",
        "cs2-yolo11n-seg.model.json",
        "cs2-yolo11n-seg-bundle.zip",
        "autonomous_report.json",
    ):
        fp = out / name
        if fp.is_file():
            display(FileLink(str(fp)))
except Exception as e:
    print("FileLink:", e)
print("Use: cs2-vision live --model … --manifest … --class-name player")
"""
        ),
        md(
            """## Notes

- **Run All** after GPU + Internet (CS2-10k auto-loads; Input zip optional).
- **No human labeling** required for the free-tier path when network works.
- Target class is **`player`** shapes/silhouettes.
- **Held-out val is default** (`allow_leaky_val=False`). `ALLOW_LEAKY_VAL=1`
  for flat train=val only when intentionally research-leaky.
- **Teacher gate**, **conf schedule**, **CS2-10k holdout** exposed in config.
- **EdgeSAM** (`USE_EDGESAM`) opt-in Vombit+EdgeSAM auto-label (default COCO bootstrap).
- Profile **`cloud_t4`**, **`resume=True`**.
- Progress cell shows `progress/report.md` + plots after train.
- Final class is **`player`** (`--class-name player` in live).
- After train, inspect `status`, `final_label_count`, `best_iteration`,
  `best_map`, `bundle_path`, `dataset_license`.
- Package: `CS2_VISION_REPO` or auto-clone via `CS2_VISION_GIT`.
"""
        ),
    ]
