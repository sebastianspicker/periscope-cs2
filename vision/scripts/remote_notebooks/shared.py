"""Shared notebook cell fragments used by Colab and Kaggle generators."""

from __future__ import annotations

_PROGRESS_CELL = r"""# @title 3b) Progress report (shapes / metrics)
from IPython.display import Markdown, Image, display
from pathlib import Path
progress = Path(getattr(report, "progress_dir", None) or Path(report.data_dir) / "progress")
md = Path(getattr(report, "progress_report_md", None) or progress / "report.md")
if md.is_file():
    display(Markdown(md.read_text(encoding="utf-8")[:50000]))
for png in sorted(progress.glob("*.png")):
    display(Image(filename=str(png)))
if not progress.is_dir():
    print("No progress/ yet; check train logs")
"""


_TRAIN_PRINT_TAIL = r"""
print("--- autonomous shape train complete ---")
print("status:", report.status)
print("final_label_count:", report.final_label_count)
print("final_image_count:", report.final_image_count)
print("onnx_path:", report.onnx_path)
print("best_onnx_path:", report.best_onnx_path)
print("best_iteration:", report.best_iteration)
print("best_map:", report.best_map)
print("bundle_path:", report.bundle_path)
print("progress_report_md:", report.progress_report_md)
print("dataset_license:", report.dataset_license)
print(
    "Live: cs2-vision live --model <onnx> --manifest <manifest> --class-name player"
)
print(json.dumps(report.as_dict(), indent=2)[:2500])
"""

# Identical train-loop body for Colab and Kaggle (DRY).
_TRAIN_LOOP_BODY = r"""# @title 3) Iterative train loop (CS2-10k capable)
import json
from cs2_vision_access.training.contracts import PRODUCT_CLASSES

epochs = None if EPOCHS_PER_ITER in (0, None) else int(EPOCHS_PER_ITER)
maps = tuple(m.strip() for m in CS2_10K_MAPS.split(",") if m.strip())

_loop_kwargs = dict(
    dataset_zip=zip_path,
    data_dir=OUTPUT_DIR,
    iterations=int(ITERATIONS),
    epochs_per_iter=epochs,
    batch=int(BATCH),
    imgsz=int(IMGSZ),
    base_model=BASE_MODEL,
    conf_threshold=float(CONF_SELF_TRAIN),
    conf_schedule=bool(CONF_SCHEDULE),
    use_teacher_gate=bool(USE_TEACHER_GATE),
    bootstrap_if_needed=bool(BOOTSTRAP),
    min_label_ratio=float(MIN_LABEL_RATIO),
    classes=PRODUCT_CLASSES,  # {0: player} shapes
    origin="Autonomous remote player-shape train (CS2-10k / YOLO-seg)",
    install_deps=False,
    allow_leaky_val=bool(ALLOW_LEAKY_VAL),
    profile="cloud_t4",
    resume=True,
    use_cs2_10k=bool(USE_CS2_10K),
    cs2_10k_maps=maps or ("mirage", "dust2"),
    cs2_10k_max_shards=int(CS2_10K_MAX_SHARDS),
    cs2_10k_max_videos=int(CS2_10K_MAX_VIDEOS),
    cs2_10k_frames_per_video=int(CS2_10K_FRAMES_PER_VIDEO),
    cs2_10k_holdout_video_fraction=float(CS2_10K_HOLDOUT_FRAC),
    continue_on_self_train_error=False,
    use_edgesam=bool(USE_EDGESAM),
    download_edgesam=bool(DOWNLOAD_EDGESAM),
    edgesam_artifacts_dir=(EDGESAM_ARTIFACTS or None),
    edgesam_confidence=float(EDGESAM_CONF),
    edgesam_coco_fallback=bool(EDGESAM_COCO_FALLBACK),
)
try:
    report = run_autonomous_loop(**_loop_kwargs)
except TypeError as e:
    # remote_autonomous may not expose EdgeSAM kwargs yet
    print(
        "Note: run_autonomous_loop does not accept EdgeSAM kwargs yet "
        f"({e}); retrying without them."
    )
    for _k in (
        "use_edgesam",
        "download_edgesam",
        "edgesam_artifacts_dir",
        "edgesam_confidence",
        "edgesam_coco_fallback",
    ):
        _loop_kwargs.pop(_k, None)
    report = run_autonomous_loop(**_loop_kwargs)
"""


def train_loop_cell_source() -> str:
    """Full train-loop cell source (body + print tail)."""
    return _TRAIN_LOOP_BODY + _TRAIN_PRINT_TAIL
