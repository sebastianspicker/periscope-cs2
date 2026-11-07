"""Post-process helpers for the GPU inference pipeline."""

from __future__ import annotations

from typing import Any

import numpy as np

from cs2_vision_access.adapters.models.runtime.cuda.decode import decode
from cs2_vision_access.adapters.models.runtime.cuda.nms import nms


def _session_input_dtype(session: Any) -> np.dtype:
    """Determine the ONNX model's actual input element dtype.

    Parses ``session.get_inputs()[0].type`` (e.g. ``"tensor(float)"`` or
    ``"tensor(float16)"``) and maps it to a NumPy dtype.  Falls back to
    ``np.float32`` when the type string is missing or unparseable.
    """
    try:
        inputs = session.get_inputs()
        if not inputs:
            return np.dtype(np.float32)
        type_str = str(getattr(inputs[0], "type", "") or "")
        element = (
            type_str[len("tensor(") : -1]
            if type_str.startswith("tensor(") and type_str.endswith(")")
            else type_str
        )
        if element in ("float16", "half"):
            return np.dtype(np.float16)
        if element == "float":
            return np.dtype(np.float32)
        if element == "double":
            return np.dtype(np.float64)
    except RuntimeError:
        # An unavailable ORT session has no reliable declared dtype; float32
        # is the established portable input fallback.
        return np.dtype(np.float32)
    return np.dtype(np.float32)


def _num_predictions(session: Any, output_name: str, image_size: int) -> int:
    """Prediction count of the detection output.

    Uses the session's static output shape when declared; otherwise derives it
    from the input size (anchors of the stride-8/16/32 grids: 8400 at 640).
    """
    for out in session.get_outputs():
        shape = getattr(out, "shape", None)
        if out.name == output_name and shape and len(shape) == 3 and isinstance(shape[2], int):
            return int(shape[2])
    return sum((image_size // stride) ** 2 for stride in (8, 16, 32))


def _decode_nms(
    raw_output: Any,
    num_classes: int,
    orig_w: int,
    orig_h: int,
    det_size: int,
    confidence: float,
    iou_threshold: float,
) -> tuple[Any, Any, Any]:
    """Decode raw YOLO output, filter by confidence, then apply NMS.

    Each operation auto-dispatches to its GPU kernel when CUDA is available
    and falls back to pure NumPy otherwise.
    """
    boxes, scores, class_ids = decode(raw_output, num_classes, orig_w, orig_h, det_size)
    boxes, scores, class_ids = _filter_by_confidence(boxes, scores, class_ids, confidence)
    if len(boxes) > 0:
        # ``nms`` runs greedy NMS on the host (CuPy inputs are copied once).
        keep = nms(boxes, scores, iou_threshold, class_ids)
        boxes = boxes[keep]
        scores = scores[keep]
        class_ids = class_ids[keep]
    return boxes, scores, class_ids


def _filter_by_confidence(
    boxes: Any,
    scores: Any,
    class_ids: Any,
    confidence: float,
) -> tuple[Any, Any, Any]:
    """Drop detections below ``confidence`` (works for NumPy and CuPy arrays)."""
    if boxes is None or len(boxes) == 0:
        return boxes, scores, class_ids
    keep = scores >= float(confidence)
    # CuPy / NumPy boolean indexing; fall back if keep is empty-shaped
    try:
        if hasattr(keep, "any") and not bool(keep.any()):
            empty_boxes = boxes[:0]
            return empty_boxes, scores[:0], class_ids[:0]
        return boxes[keep], scores[keep], class_ids[keep]
    except RuntimeError:
        import numpy as np

        boxes_np = np.asarray(boxes)
        scores_np = np.asarray(scores)
        class_ids_np = np.asarray(class_ids)
        mask = scores_np >= float(confidence)
        return boxes_np[mask], scores_np[mask], class_ids_np[mask]
