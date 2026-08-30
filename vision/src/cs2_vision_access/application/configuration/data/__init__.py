"""COCO 80-class label mapping for YOLO models.

Source of truth: ``coco80.json`` (https://cocodataset.org/#explore).
"""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path


@lru_cache(maxsize=1)
def coco80_classes() -> dict[str, str]:
    """Return the standard COCO 80-class mapping as a str→str dict."""
    path = Path(__file__).with_name("coco80.json")
    with path.open(encoding="utf-8") as handle:
        data = json.load(handle)
    return {str(k): str(v) for k, v in data.items()}


def coco80_class_names() -> tuple[str, ...]:
    """Return COCO 80 class names in id order (0..79)."""
    classes = coco80_classes()
    return tuple(classes[str(i)] for i in range(len(classes)))


__all__ = ["coco80_class_names", "coco80_classes"]
