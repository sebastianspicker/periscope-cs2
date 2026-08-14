"""Shared training logic for cloud notebook environments (Kaggle / Colab / HF Space).

This package handles:

  1. Dependency installation
  2. Dataset extraction (safe, nested-root aware)
  3. YOLO11n-seg fine-tuning
  4. ONNX export + manifest creation
  5. Download packaging

Usage::

    from cs2_vision_access.training.cloud import run_pipeline, DEFAULT_CLASSES
"""

from __future__ import annotations

from .constants import (
    DEFAULT_BASE_MODEL,
    DEFAULT_BATCH,
    DEFAULT_CLASSES,
    DEFAULT_EPOCHS,
    DEFAULT_IMGSZ,
    DEFAULT_LR0,
    DEFAULT_PATIENCE,
    OUTPUT_MANIFEST_NAME,
    OUTPUT_ONNX_FP16_NAME,
    OUTPUT_ONNX_NAME,
)
from .dataset import extract_dataset, find_dataset_zip, package_outputs
from .deps import install_dependencies
from .pipeline import create_manifest, run_pipeline
from .train import _retry_batch_on_oom, train

__all__ = [
    "DEFAULT_CLASSES",
    "DEFAULT_BASE_MODEL",
    "DEFAULT_EPOCHS",
    "DEFAULT_BATCH",
    "DEFAULT_IMGSZ",
    "DEFAULT_LR0",
    "DEFAULT_PATIENCE",
    "OUTPUT_ONNX_NAME",
    "OUTPUT_ONNX_FP16_NAME",
    "OUTPUT_MANIFEST_NAME",
    "install_dependencies",
    "find_dataset_zip",
    "package_outputs",
    "extract_dataset",
    "_retry_batch_on_oom",
    "train",
    "create_manifest",
    "run_pipeline",
]
