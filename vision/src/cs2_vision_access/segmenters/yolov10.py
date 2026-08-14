"""ONNX Runtime backend for YOLOv10 detection models.

Supports the **onnx-community/yolov10n** family of ONNX models from Hugging Face
(and any YOLOv10 ONNX exported with the NMS-free end-to-end format).

Architecture
------------
YOLOv10 differs from YOLOv5/v8/v11 in two key ways:

1. **NMS-free** — the model is trained with one-to-one matching, so the forward
   pass produces a single output tensor with final detections (no NMS post-
   processing needed).

2. **Detection-only** — YOLOv10 has no segmentation head. Outputs are bounding
   boxes, not masks. The renderer will use axis-aligned rectangle outlines.

Model format (onnx-community)
-----------------------------
- **Input**: ``images`` — float32 ``[1, 3, 640, 640]``, RGB, values in [0, 1].
- **Output**: ``output0`` — float32 ``[1, N, 6]`` where each row is
  ``[x1, y1, x2, y2, score, class_id]``. Coordinates are in the preprocessed
  (padded/square) image space and are unscaled to the original frame internally.

Preprocessing matches the ``preprocessor_config.json`` shipped with the model:
  resize longest edge → 640, pad to 640×640, rescale by 1/255.

Available variants on Hugging Face:
  ``onnx-community/yolov10n`` (2.3 M params, 40.9 MB HF repo)
  - ``onnx/model.onnx``  (FP32, 9.4 MB)
  - ``onnx/model_fp16.onnx``  (FP16, 4.7 MB)
  - ``onnx/model_quantized.onnx`` (INT8, 2.7 MB)
  - ``onnx/model_int8.onnx`` (INT8, 2.7 MB)

Usage::

    # Download from Hugging Face (manual step)
    #   git lfs clone https://huggingface.co/onnx-community/yolov10n
    #   cp yolov10n/onnx/model_quantized.onnx artifacts/yolov10n.onnx

    cs2-vision live \\
        --segmenter-backend yolov10 \\
        --model artifacts/yolov10n.onnx \\
        --manifest path/to/manifest.json \\
        --class-name person
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters._detect_common import (
    boxes_to_rectangle_masks,
    load_manifest_or_default,
    normalize_det_output,
    validate_detect_params,
)
from cs2_vision_access.segmenters.protocol import (
    YOLOV10_BACKEND,
    SegmenterError,
    resolve_class_ids,
)

# Default input name used by onnx-community exports and the original exporter.
_INPUT_NAME = "images"
# Default output name used by onnx-community exports.
_OUTPUT_NAME = "output0"

_DEFAULT_ORIGIN = "https://huggingface.co/onnx-community/yolov10n"
_DEFAULT_LICENSE = "AGPL-3.0"
_DEFAULT_MODEL_FILENAME = "yolov10n.onnx"


def _letterbox(
    frame_bgr: np.ndarray,
    target_size: int,
) -> tuple[np.ndarray, float, float, int, int]:
    """Resize and pad a BGR frame to a square, returning the RGB tensor.

    Returns:
        (rgb_tensor, scale_x, scale_y, pad_left, pad_top)
        - rgb_tensor: float32 ``[1, 3, target_size, target_size]``, RGB, [0, 1]
        - scale_x, scale_y: multipliers to map output coords → original pixels
        - pad_left, pad_top: pixels of padding added on each side
    """
    import cv2

    h, w = frame_bgr.shape[:2]

    # Resize so the longest side = target_size
    scale = target_size / max(h, w)
    new_w = int(round(w * scale))
    new_h = int(round(h * scale))

    resized = cv2.resize(frame_bgr, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

    # Pad to square
    pad_left = (target_size - new_w) // 2
    pad_top = (target_size - new_h) // 2
    pad_right = target_size - new_w - pad_left
    pad_bottom = target_size - new_h - pad_top

    padded = cv2.copyMakeBorder(
        resized,
        pad_top,
        pad_bottom,
        pad_left,
        pad_right,
        cv2.BORDER_CONSTANT,
        value=(0, 0, 0),
    )

    # BGR → RGB, HWC → CHW, uint8 → float32 [0, 1]
    rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    chw = np.transpose(rgb, (2, 0, 1))  # (C, H, W)
    batch = np.expand_dims(chw, axis=0)  # (1, C, H, W)

    # Scale factors from preprocessed space to original pixel space
    # Model outputs are in the 640×640 padded space.
    # To get original pixel coords:  orig = (output_coord - pad) / scale
    # But we apply the inverse mapping when converting boxes.
    actual_scale_x = w / new_w  # input pixels per resized pixel
    actual_scale_y = h / new_h

    return batch, actual_scale_x, actual_scale_y, pad_left, pad_top


class YoloV10Segmenter:
    """ONNX Runtime adapter for YOLOv10 detection models.

    Both the **onnx-community** Hugging Face exports and models exported from
    the THU-MIG ``yolov10`` repo are supported.

    The backend converts the NMS-free ``output0`` tensor into box-only
    ``InstanceMask`` objects (axis-aligned rectangle polygons).
    """

    def __init__(
        self,
        model_path: str | Path,
        manifest_path: str | Path,
        *,
        class_names: tuple[str, ...] | None,
        confidence: float,
        image_size: int,
        device: str,
    ) -> None:
        validate_detect_params(confidence, image_size)

        model_path = Path(model_path)
        manifest = load_manifest_or_default(
            model_path,
            manifest_path,
            model_filename=_DEFAULT_MODEL_FILENAME,
            origin=_DEFAULT_ORIGIN,
            license_name=_DEFAULT_LICENSE,
        )

        self.manifest = manifest
        self.allowed_ids = resolve_class_ids(manifest, class_names)
        self.confidence = confidence
        self.image_size = image_size
        self.device = device
        self.backend = YOLOV10_BACKEND

        try:
            from cs2_vision_access.inference.providers import create_ort_session
        except ImportError as error:
            raise SegmenterError(
                "onnxruntime is required for the yolov10 backend; install project dependencies"
            ) from error

        try:
            self._session = create_ort_session(model_path, device=device)
        except Exception as error:
            raise SegmenterError(f"could not load ONNX model {model_path!r}: {error}") from error

        # Validate input/output names.
        model_inputs = {inp.name for inp in self._session.get_inputs()}
        model_outputs = {out.name for out in self._session.get_outputs()}

        if _INPUT_NAME not in model_inputs:
            raise SegmenterError(
                f"model input {_INPUT_NAME!r} not found; available inputs: {sorted(model_inputs)}"
            )
        if _OUTPUT_NAME not in model_outputs:
            raise SegmenterError(
                f"model output {_OUTPUT_NAME!r} not found; "
                f"available outputs: {sorted(model_outputs)}"
            )

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        if frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
            raise ValueError("frame_bgr must have shape (height, width, 3)")

        height, width = frame_bgr.shape[:2]

        # Preprocess: letterbox + RGB + [0, 1]
        input_tensor, scale_x, scale_y, pad_left, pad_top = _letterbox(
            frame_bgr,
            self.image_size,
        )

        # Inference.
        try:
            outputs = self._session.run(
                [_OUTPUT_NAME],
                {_INPUT_NAME: input_tensor},
            )
        except Exception as error:
            raise SegmenterError(f"inference failed: {error}") from error

        if not outputs or outputs[0] is None:
            return ()

        boxes, scores, class_ids = normalize_det_output(outputs[0])

        # Map coordinates from padded model space → original frame pixels.
        # unmapped = (model_coord - pad) * scale
        boxes = boxes.copy()
        boxes[:, [0, 2]] = (boxes[:, [0, 2]] - pad_left) * scale_x
        boxes[:, [1, 3]] = (boxes[:, [1, 3]] - pad_top) * scale_y

        return boxes_to_rectangle_masks(
            boxes,
            scores,
            class_ids,
            frame_index=frame_index,
            width=width,
            height=height,
            manifest=self.manifest,
            allowed_ids=self.allowed_ids,
            confidence=self.confidence,
        )
