"""Optional SAM backend loaders for box-to-mask bootstrap."""

from __future__ import annotations

import os
from collections.abc import Callable
from pathlib import Path

from cs2_vision_access.labeling.convert import _clamp01, _polygon_area
from cs2_vision_access.labeling.types import BootstrapError, DetectionBox

_SAM_INSTALL_NOTE = (
    "SAM backend requires an optional install and a local checkpoint "
    "(no auto-download). Install sam2 from https://github.com/facebookresearch/sam2 "
    "or ensure Ultralytics SAM is available, set CS2_VISION_SAM_MODEL to a local "
    ".pt/.pth path, then re-run. Use --backend rectangle|ellipse for offline draft "
    "polygons without SAM. SAM is not a hard dependency of this package."
)


def polygon_for_box_sam(box: DetectionBox, *, image_path: Path) -> list[float]:
    """Prompt optional SAM with a box; fail closed without install/checkpoint.

    Looks up ``_load_sam_impl`` on the package module so unit tests can patch
    ``cs2_vision_access.labeling._load_sam_impl``.
    """
    # Late package lookup preserves historical patch target on the package.
    from cs2_vision_access import labeling as labeling_pkg

    sam_impl = labeling_pkg._load_sam_impl()
    model_path = os.environ.get("CS2_VISION_SAM_MODEL", "").strip()
    if not model_path:
        raise RuntimeError(_SAM_INSTALL_NOTE)
    checkpoint = Path(model_path)
    if checkpoint.is_symlink():
        raise BootstrapError("SAM checkpoint must not be a symlink")
    if not checkpoint.is_file():
        raise RuntimeError(
            f"CS2_VISION_SAM_MODEL is not a regular file: {checkpoint}. {_SAM_INSTALL_NOTE}"
        )
    if image_path.is_symlink():
        raise BootstrapError(f"image path must not be a symlink: {image_path}")
    return sam_impl(checkpoint, image_path, box)


def load_sam_impl() -> Callable[[Path, Path, DetectionBox], list[float]]:
    """Resolve an optional SAM implementation or raise with install note."""
    try:
        from ultralytics import SAM  # type: ignore
    except ImportError:
        SAM = None  # type: ignore
    else:

        def _ultralytics_sam(checkpoint: Path, image_path: Path, box: DetectionBox) -> list[float]:
            try:
                import cv2  # type: ignore
            except ImportError as error:  # pragma: no cover
                raise RuntimeError(
                    "OpenCV is required for the SAM backend to read image size"
                ) from error
            image = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
            if image is None:
                raise BootstrapError(f"OpenCV could not decode image: {image_path}")
            height, width = image.shape[:2]
            if height <= 0 or width <= 0:
                raise BootstrapError(f"invalid image dimensions: {image_path}")
            x_min = (box.x_center - box.width / 2.0) * width
            y_min = (box.y_center - box.height / 2.0) * height
            x_max = (box.x_center + box.width / 2.0) * width
            y_max = (box.y_center + box.height / 2.0) * height
            model = SAM(str(checkpoint))
            results = model(
                str(image_path),
                bboxes=[[x_min, y_min, x_max, y_max]],
                verbose=False,
            )
            if not results:
                raise BootstrapError(f"SAM returned no result for {image_path}")
            result = results[0]
            masks = getattr(result, "masks", None)
            if masks is None or getattr(masks, "xy", None) is None or len(masks.xy) == 0:
                raise BootstrapError(f"SAM returned no mask polygon for box in {image_path}")
            polygon_xy = masks.xy[0]
            coordinates: list[float] = []
            for point in polygon_xy:
                x_px, y_px = float(point[0]), float(point[1])
                coordinates.append(_clamp01(x_px / width))
                coordinates.append(_clamp01(y_px / height))
            if len(coordinates) < 6 or _polygon_area(coordinates) == 0.0:
                raise BootstrapError("SAM polygon is degenerate")
            return coordinates

        return _ultralytics_sam

    try:
        import sam2  # type: ignore  # noqa: F401
    except ImportError as error:
        raise RuntimeError(_SAM_INSTALL_NOTE) from error

    raise RuntimeError(
        "sam2 is importable but the integrated prompt path expects Ultralytics "
        f"SAM for box prompts. {_SAM_INSTALL_NOTE}"
    )


# Private aliases matching the former monofile names (tests patch these).
_load_sam_impl = load_sam_impl
_polygon_for_box_sam = polygon_for_box_sam
