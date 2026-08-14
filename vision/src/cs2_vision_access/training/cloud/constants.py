"""Shared constants for cloud notebook training (Kaggle / Colab / HF Space)."""

from __future__ import annotations

from .fallbacks import _PROFILES, _VOMBIT_CLASSES

# Default bootstrap / Vombit class map (id → name). Alias of contracts.VOMBIT_CLASSES.
DEFAULT_CLASSES: dict[int, str] = dict(_VOMBIT_CLASSES)

# Shared cloud training profile (Colab T4 / Kaggle / HF Space defaults).
if _PROFILES is not None:
    _cloud_profile = _PROFILES["cloud_t4"]
    DEFAULT_BASE_MODEL = _cloud_profile.base_model
    DEFAULT_EPOCHS = _cloud_profile.epochs
    DEFAULT_BATCH = _cloud_profile.batch  # safer VRAM default across free-tier GPUs
    DEFAULT_IMGSZ = _cloud_profile.image_size
    DEFAULT_LR0 = _cloud_profile.lr0
    DEFAULT_PATIENCE = _cloud_profile.patience
else:
    DEFAULT_BASE_MODEL = "yolo11n-seg.pt"
    DEFAULT_EPOCHS = 150
    DEFAULT_BATCH = 16
    DEFAULT_IMGSZ = 416
    DEFAULT_LR0 = 0.001
    DEFAULT_PATIENCE = 50

OUTPUT_ONNX_NAME = "cs2-yolo11n-seg.onnx"
OUTPUT_ONNX_FP16_NAME = "cs2-yolo11n-seg-fp16.onnx"
OUTPUT_MANIFEST_NAME = "cs2-yolo11n-seg.model.json"

# Common locations where uploaded / attached dataset zips appear.
_DEFAULT_ZIP_SEARCH_ROOTS: tuple[str, ...] = (
    "/kaggle/input",
    "/content",
    "/content/drive/MyDrive",
    ".",
    "data",
)
