"""Supervision integration — convert between InstanceMask and sv.Detections.

Provides a SupervisionAnnotator that wraps supervision annotators (MaskAnnotator,
BoxAnnotator, LabelAnnotator) as an alternative rendering path alongside the
custom dual-stroke outline renderer.

Requires ``pip install supervision`` (optional dependency).
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
from typing import Any

import numpy as np

from cs2_vision_access.domain.predictions import InstanceMask

# Optional hard deps kept as module attributes so tests can patch them.
try:
    import cv2
except ImportError:  # pragma: no cover
    cv2 = None  # type: ignore[assignment]

try:
    import supervision as sv
except ImportError:  # pragma: no cover
    sv = None  # type: ignore[assignment]


_INSTALL_HINT = (
    "supervision is required for Supervision-based annotators; "
    "install with: pip install supervision"
)


class SupervisionBridgeError(RuntimeError):
    """Supervision integration failed."""


@dataclass(frozen=True)
class SupervisionAnnotatorConfig:
    """Configuration for Supervision-based rendering.

    ``mask`` — draw instance masks (uses sv.MaskAnnotator).
    ``box`` — draw bounding boxes (uses sv.BoxAnnotator).
    ``label`` — draw class + confidence labels (uses sv.LabelAnnotator).
    ``trace`` — draw trace trails (uses sv.TraceAnnotator, experimental).
    ``color_lookup`` — color assignment strategy (INDEX, CLASS, TRACK).
    """

    mask: bool = False
    box: bool = False
    label: bool = False
    trace: bool = False
    color_lookup: str = "INDEX"


class SupervisionAnnotator:
    """Renders using supervision annotators as a drop-in for OutlineRenderer.

    Usage::

        annotator = SupervisionAnnotator()
        rendered = annotator.render(frame_bgr, predictions, frame_index=0)

    This is an alternative research path alongside the custom dual-stroke
    renderer. The dual-stroke path remains the primary accessibility treatment.
    """

    def __init__(
        self,
        config: SupervisionAnnotatorConfig | None = None,
    ) -> None:
        self._config = config or SupervisionAnnotatorConfig()
        if sv is None:
            raise SupervisionBridgeError(_INSTALL_HINT)
        self._sv = sv
        self._annotators: list[Any] = []
        self._setup_annotators()

    def _setup_annotators(self) -> None:
        sv = self._sv
        color_lookup = getattr(
            sv.ColorLookup, self._config.color_lookup.upper(), sv.ColorLookup.INDEX
        )
        if self._config.box:
            self._annotators.append(sv.BoxAnnotator(color_lookup=color_lookup))
        if self._config.mask:
            self._annotators.append(sv.MaskAnnotator(color_lookup=color_lookup))
        if self._config.label:
            self._annotators.append(sv.LabelAnnotator(color_lookup=color_lookup))
        if self._config.trace:
            self._annotators.append(sv.TraceAnnotator(color_lookup=color_lookup))

    def render(
        self,
        frame_bgr: np.ndarray,
        predictions: Sequence[InstanceMask],
        *,
        frame_index: int,
    ) -> np.ndarray:
        """Render supervision annotations onto a BGR frame copy.

        Only masks with ``frame_index`` matching the current frame are rendered.
        """
        current = [p for p in predictions if p.frame_index == frame_index]
        if not current:
            return frame_bgr.copy()

        detections = instance_masks_to_detections(current, frame=frame_bgr)
        output = frame_bgr.copy()
        for annotator in self._annotators:
            output = annotator.annotate(
                scene=output,
                detections=detections,
            )
        return output


def instance_masks_to_detections(
    masks: Sequence[InstanceMask],
    *,
    with_labels: bool = True,
    frame: np.ndarray | None = None,
) -> Any:
    """Convert InstanceMask sequence to a supervision Detections object.

    Args:
        masks: Sequence of InstanceMask predictions.
        with_labels: If True, use class_name for label annotations.
        frame: Optional BGR frame to resolve mask rasterization shape.

    Returns:
        ``sv.Detections`` object with fields: xyxy, mask, confidence, class_id, tracker_id.

    Raises:
        SupervisionBridgeError: if supervision is not installed.
    """
    if sv is None:
        raise SupervisionBridgeError(_INSTALL_HINT)

    if not masks:
        return sv.Detections.empty()

    height, width = (frame.shape[:2]) if frame is not None else (720, 1280)

    xyxy_list: list[list[float]] = []
    mask_list: list[np.ndarray] = []
    confidence_list: list[float] = []
    class_id_list: list[int] = []
    class_name_list: list[str] = []

    for m in masks:
        polygon = np.asarray(m.polygon, dtype=np.float32)
        if len(polygon) < 3:
            continue
        xs = polygon[:, 0]
        ys = polygon[:, 1]
        x1, y1 = float(np.min(xs)), float(np.min(ys))
        x2, y2 = float(np.max(xs)), float(np.max(ys))
        xyxy_list.append([x1, y1, x2, y2])

        mask_img = np.zeros((height, width), dtype=np.uint8)
        contour = np.rint(polygon).astype(np.int32).reshape((-1, 1, 2))
        if cv2 is None:
            raise SupervisionBridgeError("OpenCV is required for mask rasterization")
        cv2.fillPoly(mask_img, [contour], 1)
        mask_list.append(mask_img.astype(bool))

        confidence_list.append(m.confidence)
        class_id_list.append(m.class_id)
        class_name_list.append(m.class_name)

    if not xyxy_list:
        return sv.Detections.empty()

    data: dict[str, Any] = {"class_name": np.array(class_name_list, dtype=object)}
    if with_labels:
        data["label"] = np.array(
            [
                f"{name} {conf:.2f}"
                for name, conf in zip(class_name_list, confidence_list, strict=False)
            ],
            dtype=object,
        )

    return sv.Detections(
        xyxy=np.array(xyxy_list, dtype=np.float32),
        mask=np.array(mask_list, dtype=bool),
        confidence=np.array(confidence_list, dtype=np.float32),
        class_id=np.array(class_id_list, dtype=int),
        data=data,
    )


def detections_to_instance_masks(
    detections: Any,
    *,
    frame_index: int,
    manifest_classes: dict[int, str] | None = None,
) -> tuple[InstanceMask, ...]:
    """Convert supervision Detections back to InstanceMask tuples.

    This is useful when using supervision-based processing (tracking, filtering)
    before passing to the dual-stroke renderer.

    Args:
        detections: ``sv.Detections`` object.
        frame_index: Frame index to stamp on all masks.
        manifest_classes: Optional mapping of class_id → class_name. When None,
            class names are read from ``detections.data["class_name"]``.

    Returns:
        Tuple of InstanceMask objects.
    """
    if sv is None:
        raise SupervisionBridgeError(_INSTALL_HINT)

    if not isinstance(detections, sv.Detections) or len(detections) == 0:
        return ()

    class_names_data = detections.data.get("class_name")
    results: list[InstanceMask] = []
    for idx in range(len(detections)):
        class_id = int(detections.class_id[idx]) if detections.class_id is not None else 0
        confidence = float(detections.confidence[idx]) if detections.confidence is not None else 1.0

        if manifest_classes is not None and class_id in manifest_classes:
            class_name = manifest_classes[class_id]
        elif (
            class_names_data is not None
            and idx < len(class_names_data)
            and class_names_data[idx] is not None
        ):
            class_name = str(class_names_data[idx])
        else:
            class_name = f"class_{class_id}"

        mask_bool: np.ndarray | None = (
            np.asarray(detections.mask[idx]) if detections.mask is not None else None
        )
        if mask_bool is not None and mask_bool.any():
            if cv2 is None:
                raise SupervisionBridgeError("OpenCV is required for mask-to-polygon conversion")
            mask_uint8 = mask_bool.astype(np.uint8) * 255
            contours, _ = cv2.findContours(mask_uint8, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            if not contours:
                continue
            contour = max(contours, key=cv2.contourArea)
            points = np.asarray(contour, dtype=np.float32).reshape(-1, 2)
            if len(points) < 3:
                continue
            polygon: tuple[tuple[float, float], ...] = tuple(
                (float(x), float(y)) for x, y in points
            )
        else:
            xyxy = detections.xyxy[idx]
            polygon = (
                (float(xyxy[0]), float(xyxy[1])),
                (float(xyxy[2]), float(xyxy[1])),
                (float(xyxy[2]), float(xyxy[3])),
                (float(xyxy[0]), float(xyxy[3])),
            )

        results.append(
            InstanceMask(
                frame_index=frame_index,
                polygon=polygon,
                confidence=confidence,
                class_id=class_id,
                class_name=class_name,
            )
        )
    return tuple(results)
