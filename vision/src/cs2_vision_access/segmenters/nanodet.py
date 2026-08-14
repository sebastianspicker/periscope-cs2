"""ONNX Runtime backend for NanoDet / NanoDet-Plus detection models.

NanoDet is an ultra-lightweight FCOS-style anchor-free detector from
`RangiLyu/nanodet <https://github.com/RangiLyu/nanodet>`_.  The smallest
variant (NanoDet-Plus-m-320) has only **1.17 M params** and a **2.3 MB FP16**
ONNX file — roughly 4× smaller than YOLO11n-seg.

Architecture
------------
- FCOS-style anchor-free detection head with Generalized Focal Loss.
- The ONNX export can optionally bake in NMS (via ``tools/export_onnx.py``).
- Detection only — outputs bounding boxes with no segmentation masks.

ONNX export (required before use)
-----------------------------------
The Hugging Face Hub does **not** carry pre-exported NanoDet models.  You must
export ONNX from a PyTorch checkpoint yourself::

    # Clone the repo and set up the environment.
    git clone https://github.com/RangiLyu/nanodet.git
    cd nanodet
    pip install -r requirements.txt
    python setup.py develop

    # Download a pretrained checkpoint (e.g. NanoDet-Plus-m-320).
    # From the Model Zoo in the repo README, download the .ckpt file.

    # Export to ONNX (NMS included in the graph).
    python tools/export_onnx.py \\
        --cfg_path config/nanodet-plus-m_320.yml \\
        --model_path /path/to/nanodet-plus-m_320_checkpoint.ckpt

    # The exported model is saved alongside the checkpoint as ``.onnx``.

The exported ONNX has:
- **Input**: ``input.1`` — float32 ``[1, 3, H, W]``, **BGR**, values 0–255,
  where ``H, W`` match the training resolution (e.g. 320, 416).
- **Output**: a single float32 tensor ``[1, N, 6]`` where each row is
  ``[x1, y1, x2, y2, score, class_id]`` — coordinates are in **original image
  pixel space** (not padded), so no coordinate unprojection is needed.

NanoDet-Plus variants from the Model Zoo:

+-------------------------------+---------+--------+--------+----------+
| Model                         | Input   | Params | mAP    | ONNX sz |
+-------------------------------+---------+--------+--------+----------+
| NanoDet-Plus-m-320            | 320×320 | 1.17 M | 27.0   | 2.3 MB  |
| NanoDet-Plus-m-416            | 416×416 | 1.17 M | 30.4   | 2.3 MB  |
| NanoDet-Plus-m-1.5x-320       | 320×320 | 2.44 M | 29.9   | 4.7 MB  |
| NanoDet-Plus-m-1.5x-416       | 416×416 | 2.44 M | 34.1   | 4.7 MB  |
+-------------------------------+---------+--------+--------+----------+

The model was trained on COCO and supports all 80 COCO classes (class 0 =
``person``).

Usage::

    cs2-vision live \\
        --segmenter-backend nanodet \\
        --model artifacts/nanodet-plus-m-320.onnx \\
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
    NANODET_BACKEND,
    SegmenterError,
    resolve_class_ids,
)

# Default input / output names for NanoDet ONNX exports.
_INPUT_NAME = "input.1"
_OUTPUT_NAMES = ("output", "detections", "transpose_22.tmp_0")

_DEFAULT_ORIGIN = "https://github.com/RangiLyu/nanodet"
_DEFAULT_LICENSE = "Apache-2.0"
_DEFAULT_MODEL_FILENAME = "nanodet.onnx"


class NanoDetSegmenter:
    """ONNX Runtime adapter for NanoDet / NanoDet-Plus detection models.

    The backend converts the detection tensor into box-only ``InstanceMask``
    objects (axis-aligned rectangle polygons).

    The exported ONNX must include NMS in the graph (the default when using
    ``tools/export_onnx.py`` without ``--keep-head``).
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
        self.backend = NANODET_BACKEND

        try:
            from cs2_vision_access.inference.providers import create_ort_session
        except ImportError as error:
            raise SegmenterError(
                "onnxruntime is required for the nanodet backend; install project dependencies"
            ) from error

        try:
            self._session = create_ort_session(model_path, device=device)
        except Exception as error:
            raise SegmenterError(f"could not load ONNX model {model_path!r}: {error}") from error

        # Detect the actual input and output names.
        self._input_name: str = _INPUT_NAME
        model_inputs = {inp.name for inp in self._session.get_inputs()}
        if _INPUT_NAME not in model_inputs:
            # Try to find any plausible input.
            if len(model_inputs) == 1:
                self._input_name = next(iter(model_inputs))
            else:
                raise SegmenterError(
                    f"model input {_INPUT_NAME!r} not found; "
                    f"available inputs: {sorted(model_inputs)}"
                )

        model_outputs = {out.name for out in self._session.get_outputs()}
        # Find the NMS output — should be a 2D or 3D float tensor with 6 channels.
        self._output_name: str | None = None
        for candidate in _OUTPUT_NAMES:
            if candidate in model_outputs:
                self._output_name = candidate
                break
        if self._output_name is None:
            # Fall back to the first float32 output.
            for out in self._session.get_outputs():
                if out.type and "float" in out.type.lower():
                    self._output_name = out.name
                    break
        if self._output_name is None:
            raise SegmenterError(
                f"could not determine model output; available outputs: {sorted(model_outputs)}"
            )

    def predict(self, frame_bgr: np.ndarray, *, frame_index: int) -> tuple[InstanceMask, ...]:
        if frame_index < 0:
            raise ValueError("frame_index must be non-negative")
        if frame_bgr.ndim != 3 or frame_bgr.shape[2] != 3:
            raise ValueError("frame_bgr must have shape (height, width, 3)")

        height, width = frame_bgr.shape[:2]

        # Preprocess: resize to model input size (stretch BGR).
        input_tensor = _preprocess(frame_bgr, self.image_size)

        # Inference.
        try:
            outputs = self._session.run(
                [self._output_name],
                {self._input_name: input_tensor},
            )
        except Exception as error:
            raise SegmenterError(f"inference failed: {error}") from error

        if not outputs or outputs[0] is None:
            return ()

        boxes, scores, class_ids = normalize_det_output(
            outputs[0],
            allow_7_cols=True,
        )
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


def _preprocess(frame_bgr: np.ndarray, target_size: int) -> np.ndarray:
    """Resize a BGR frame to the model input size, returning a batch tensor.

    NanoDet ONNX exports typically expect:
    - BGR channel order (no conversion to RGB needed)
    - 0–255 uint8 or float32 range
    - Resized to the training resolution (may letterbox or stretch)

    The exported model from ``tools/export_onnx.py`` stretches to the exact
    dimensions (not letterboxed), because the training config controls the
    image size directly.

    Returns:
        float32 tensor ``[1, 3, target_size, target_size]``
    """
    import cv2

    resized = cv2.resize(frame_bgr, (target_size, target_size), interpolation=cv2.INTER_LINEAR)
    # HWC → CHW, keep as float32 (0–255 range as the model expects)
    chw = np.transpose(resized.astype(np.float32), (2, 0, 1))
    return np.expand_dims(chw, axis=0)
