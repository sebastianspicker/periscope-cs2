"""Colab notebook cell definitions."""

from __future__ import annotations

from .format import code, md
from .install import _colab_install_cell
from .shared import _PROGRESS_CELL, train_loop_cell_source


def build_colab_cells() -> list[dict]:
    return [
        md(
            """# CS2 player shape training (Colab)

Train YOLO-seg for CS2 player silhouettes (class `player`).

Select a T4 GPU (Runtime → Change runtime type), then Run all.

Iterative free-tier path when the network is available:

1. Load data (CS2-10k slice or your zip) into images
2. Bootstrap shapes (COCO person mapped to class `player` if labels are sparse)
3. Fine-tune YOLO-seg (`cloud_t4`, `resume=True`)
4. Self-label high-confidence student masks (optional teacher gate)
5. Retrain for N iterations

Validation defaults to a held-out train/val split (`allow_leaky_val=False`).
Optional leaky train=val only via `ALLOW_LEAKY_VAL=1` (research).

| Knob | Default |
|------|---------|
| Iterations | 4 |
| Profile | `cloud_t4` |
| Epochs / iter | 0 (use profile epochs) |
| Conf self-train | 0.45 (+ schedule) |
| Teacher gate | `True` (prev best) |
| CS2-10k holdout | 0.2 of videos |
| Batch | 16 |
| Image size | 416 |
| Resume mid-loop | `True` |

Data (default): download a small slice of
[RekaAI/CS2-10k](https://huggingface.co/datasets/RekaAI/CS2-10k) (CC BY-NC),
sample frames, bootstrap silhouettes, then iterate. Or set `ZIP_PATH` / upload.

Labels: default bootstrap is COCO person mapped to `player`. Set
`USE_EDGESAM=True` for Vombit+EdgeSAM teachers (downloads more ONNX assets;
more disk and time).
"""
        ),
        code(_colab_install_cell()),
        code(
            r"""# @title 2) Config (edit once, then Run all)
# Target class: **player** silhouettes / shapes (PRODUCT_CLASSES {0: player})

ZIP_PATH = os.environ.get("CS2_DATASET_ZIP", "")  # optional local zip
MOUNT_DRIVE = False  # @param {type:"boolean"}
AUTO_UPLOAD_IF_MISSING = False  # @param {type:"boolean"}

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
OUTPUT_DIR = "/content/cs2_data"  # @param {type:"string"}
AUTO_DOWNLOAD = True  # @param {type:"boolean"}
# Profile default is cloud_t4; mid-loop resume defaults True in run_autonomous_loop.
# Held-out val is the default (allow_leaky_val=False). To force leaky train=val
# for quick research only: set env ALLOW_LEAKY_VAL=1 or True below.
ALLOW_LEAKY_VAL = os.environ.get("ALLOW_LEAKY_VAL", "").strip().lower() in (
    "1", "true", "yes",
)  # @param {type:"boolean"}

# --- EdgeSAM auto-label (opt-in Vombit+EdgeSAM) ---
USE_EDGESAM = False  # @param {type:"boolean"}  # opt-in: Vombit+EdgeSAM auto-label
DOWNLOAD_EDGESAM = True  # @param {type:"boolean"}  # download teachers if missing
EDGESAM_ARTIFACTS = ""  # @param {type:"string"}  # empty = data_dir/edgesam_assets
EDGESAM_CONF = 0.4  # @param {type:"number"}
EDGESAM_COCO_FALLBACK = True  # @param {type:"boolean"}

if MOUNT_DRIVE:
    from google.colab import drive
    drive.mount("/content/drive", force_remount=False)
    print("Drive mounted")

zip_path = resolve_remote_dataset_zip(
    ZIP_PATH or None,
    search_roots=[
        "/content",
        "/content/drive/MyDrive",
        "/content/drive/MyDrive/cs2",
        ".",
        "data",
    ],
)

if zip_path is None and AUTO_UPLOAD_IF_MISSING and not USE_CS2_10K:
    from google.colab import files
    print("Upload dataset zip (images + optional labels)...")
    up = files.upload()
    if not up:
        raise SystemExit("No zip uploaded and none found on disk.")
    zip_path = Path(list(up.keys())[0])

if zip_path is None and not USE_CS2_10K:
    raise SystemExit("Enable USE_CS2_10K (default) or provide ZIP_PATH / upload.")

print("Target class: player silhouettes / shapes")
print("Dataset zip:", zip_path if zip_path else "(none; RekaAI/CS2-10k)")
print(f"CS2-10k: {USE_CS2_10K} maps={CS2_10K_MAPS} shards={CS2_10K_MAX_SHARDS}")
print(f"  holdout_video_fraction={CS2_10K_HOLDOUT_FRAC}")
print(
    f"Loop: {ITERATIONS} iters, batch={BATCH}, imgsz={IMGSZ}, model={BASE_MODEL}"
)
print(
    f"Self-train: conf={CONF_SELF_TRAIN} schedule={CONF_SCHEDULE} "
    f"teacher_gate={USE_TEACHER_GATE}"
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
            r"""# @title 4) Drive copy + browser download
from pathlib import Path
import shutil

out = Path(OUTPUT_DIR)
artifacts = [
    out / "cs2-yolo11n-seg.onnx",
    out / "cs2-yolo11n-seg-fp16.onnx",
    out / "cs2-yolo11n-seg.model.json",
    out / "cs2-yolo11n-seg-bundle.zip",
    out / "autonomous_report.json",
]

if Path("/content/drive/MyDrive").exists():
    drive_out = Path("/content/drive/MyDrive/cs2_train_outputs")
    drive_out.mkdir(parents=True, exist_ok=True)
    for p in artifacts:
        if p.is_file():
            shutil.copy2(p, drive_out / p.name)
            print("Drive ←", p.name)

if AUTO_DOWNLOAD:
    from google.colab import files
    for p in artifacts:
        if p.is_file():
            print("Downloading", p.name)
            files.download(str(p))

print("Use: cs2-vision live --model … --manifest … --class-name player")
"""
        ),
        md(
            """## Notes

- **Run all** is enough after GPU (CS2-10k auto-loads; zip optional).
- **No human labeling** required for the free-tier path when network works.
- Target class is **`player`** shapes/silhouettes.
- **Held-out val is default** (`allow_leaky_val=False`). Set `ALLOW_LEAKY_VAL=1`
  only for flat train=val research shortcuts.
- **Teacher gate** (`USE_TEACHER_GATE`) uses prev-best ONNX as multi-iter teacher.
- **EdgeSAM** (`USE_EDGESAM`) opt-in Vombit+EdgeSAM auto-label (default COCO bootstrap).
- **Conf schedule** raises self-train conf over iterations when enabled.
- **CS2-10k holdout** (`CS2_10K_HOLDOUT_FRAC`) reserves videos for val.
- Profile **`cloud_t4`**, **`resume=True`** (mid-loop state under data dir).
- Progress cell shows `progress/report.md` + plots after train.
- After train, inspect `report.status`, `final_label_count`, `best_iteration`,
  `best_map`, `bundle_path`, `dataset_license`.
- `MOUNT_DRIVE=True` persists outputs across disconnects.
- Package install: `CS2_VISION_REPO=git+https://…` or auto-clone via
  `CS2_VISION_GIT` (fallback placeholder if unset).
"""
        ),
    ]
