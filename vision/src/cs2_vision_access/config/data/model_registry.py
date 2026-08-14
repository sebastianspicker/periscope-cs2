"""Recommended model registry for ``cs2-vision download-model``.

Each entry maps a short name to model metadata including download URL,
SHA-256 pin, class mapping, recommended usage, and search tags. The
``tags`` key powers ``list_models(tag=...)``; the pinned ``sha256`` enables
post-download integrity verification in ``_handle_onnx_direct``.

The registry is versioned — entries may be added over time but existing
entries are never removed without a major version bump.
"""

from __future__ import annotations

from typing import Any

MODEL_REGISTRY: dict[str, dict[str, Any]] = {
    # ── YOLO11n-seg (COCO, generic person detection) ──────────────────
    "yolo11n-seg": {
        "description": "YOLO11 nano instance segmentation — COCO person class",
        "format": "onnx",
        "url": "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n-seg.onnx",
        "sha256": "0bc32bc92e985b881141ef9bd2216e2a746f70519d0d24da9fc85decc4428cf4",
        "task": "instance-segmentation",
        "imgsz": 640,
        "classes": {"0": "person"},
        "license": "AGPL-3.0",
        "recommended": False,
        "tags": ["coco", "person", "yolo11", "segment", "generic"],
    },
    # ── Vombit CS2 detector ──────────────────────────────────────────
    "vombit-yolov10n": {
        "description": "Vombit YOLOv10n — CS2 player detection (FP32)",
        "format": "onnx",
        "url": "https://huggingface.co/Vombit/yolov10n_cs2/resolve/main/yolov10n_cs2.onnx",
        "sha256": "62fd816a1218cf77725ed85692ae2e4df2a204dc14554fdf82b973ff5b47ea00",
        "task": "detect",
        "imgsz": 640,
        "classes": {"0": "ct", "1": "ct_head", "2": "t", "3": "t_head"},
        "license": "CC-BY-NC-ND-4.0",
        "recommended": True,
        "tags": ["cs2", "vombit", "yolov10", "detect", "fast"],
    },
    "vombit-yolov10n-fp16": {
        "description": "Vombit YOLOv10n — CS2 player detection (FP16, 2× faster GPU)",
        "format": "onnx",
        "url": "https://huggingface.co/Vombit/yolov10n_cs2/resolve/main/yolov10n_cs2_fp16.onnx",
        "sha256": "d49d03099ff9aa75b8caa23f9268153b7c84e75dc555eb1475510b22b12213bd",
        "task": "detect",
        "imgsz": 640,
        "classes": {"0": "ct", "1": "ct_head", "2": "t", "3": "t_head"},
        "license": "CC-BY-NC-ND-4.0",
        "recommended": True,
        "tags": ["cs2", "vombit", "yolov10", "detect", "fp16", "fast"],
    },
    "vombit-yolov10s": {
        "description": "Vombit YOLOv10s — CS2 player detection (small, more accurate)",
        "format": "onnx",
        "url": "https://huggingface.co/Vombit/yolov10s_cs2/resolve/main/yolov10s_cs2.onnx",
        "sha256": "13fa954ebcc75917760be8d7916cb799ffcec8a9b8152e020e074d7887d3cd84",
        "task": "detect",
        "imgsz": 640,
        "classes": {"0": "ct", "1": "ct_head", "2": "t", "3": "t_head"},
        "license": "CC-BY-NC-ND-4.0",
        "recommended": False,
        "tags": ["cs2", "vombit", "yolov10", "detect", "accurate"],
    },
    # ── EdgeSAM ──────────────────────────────────────────────────────
    "edgesam-encoder": {
        "description": "EdgeSAM image encoder (for cs2-sam backend)",
        "format": "onnx",
        "url": "https://huggingface.co/chongzhou/EdgeSAM/resolve/main/edge_sam_3x_encoder.onnx",
        "sha256": "719a498cf5b3fe9be9f01ee513e13d3915f9028aa4f23dfd30eaaa0a17143159",
        "task": "segment",
        "imgsz": 1024,
        "license": "MIT",
        "recommended": True,
        "tags": ["sam", "edgesam", "segment", "encoder"],
    },
    "edgesam-decoder": {
        "description": "EdgeSAM mask decoder (for cs2-sam backend)",
        "format": "onnx",
        "url": "https://huggingface.co/chongzhou/EdgeSAM/resolve/main/edge_sam_3x_decoder.onnx",
        "sha256": "83a2174d54571596913dcb7455d021e713623c3dca30a31c8c41ab98c9fb0863",
        "task": "segment",
        "imgsz": 1024,
        "license": "MIT",
        "recommended": True,
        "tags": ["sam", "edgesam", "segment", "decoder"],
    },
}


def list_models(tag: str | None = None) -> list[tuple[str, str]]:
    """List available models, optionally filtered by tag.

    Returns ``[(name, description), ...]`` sorted by name.
    """
    results = []
    for name, info in sorted(MODEL_REGISTRY.items()):
        if tag and tag not in info.get("tags", []):
            continue
        results.append((name, info["description"]))
    return results


def get_model_info(name: str) -> dict[str, Any] | None:
    """Return metadata for a named model, or ``None`` if not found."""
    return MODEL_REGISTRY.get(name)


__all__ = [
    "MODEL_REGISTRY",
    "get_model_info",
    "list_models",
]
