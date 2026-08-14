"""Box-to-mask bootstrap: convert YOLO detection boxes into draft seg polygons.

Detection labels (``class_id x_c y_c w h``, normalized) become Ultralytics
segmentation polygons (``class_id x1 y1 ... xn yn``). Outputs are always
``draft_pending`` — never ground truth — and require human review.
"""

from __future__ import annotations

from cs2_vision_access.labeling.convert import (
    box_to_ellipse_polygon,
    box_to_rectangle_polygon,
    format_segmentation_row,
    parse_detection_label,
)
from cs2_vision_access.labeling.pipeline import boxes_to_masks
from cs2_vision_access.labeling.promote import promote_drafts
from cs2_vision_access.labeling.review import (
    accept_drafts,
    list_draft_status,
    reject_drafts,
)
from cs2_vision_access.labeling.review_io import (
    load_draft_status,
    save_draft_status,
)
from cs2_vision_access.labeling.sam import (
    _SAM_INSTALL_NOTE,
    _load_sam_impl,
    load_sam_impl,
    polygon_for_box_sam,
)
from cs2_vision_access.labeling.types import (
    DEFAULT_BACKEND,
    DEFAULT_OUTPUT_CLASS_ID,
    DRAFT_STATUS_FILENAME,
    ELLIPSE_POINT_COUNT,
    REVIEW_STATUS_ACCEPTED,
    REVIEW_STATUS_DRAFT_PENDING,
    REVIEW_STATUS_PROMOTED,
    REVIEW_STATUS_REJECTED,
    REVIEW_STATUSES,
    SCHEMA_VERSION,
    SUPPORTED_BACKENDS,
    BootstrapError,
    BootstrapSummary,
    ClassMap,
    DetectionBox,
    parse_backend,
    parse_class_map,
)

__all__ = [
    "SCHEMA_VERSION",
    "DRAFT_STATUS_FILENAME",
    "REVIEW_STATUS_DRAFT_PENDING",
    "REVIEW_STATUS_ACCEPTED",
    "REVIEW_STATUS_REJECTED",
    "REVIEW_STATUS_PROMOTED",
    "REVIEW_STATUSES",
    "DEFAULT_OUTPUT_CLASS_ID",
    "ELLIPSE_POINT_COUNT",
    "SUPPORTED_BACKENDS",
    "DEFAULT_BACKEND",
    "BootstrapError",
    "BootstrapSummary",
    "ClassMap",
    "DetectionBox",
    "accept_drafts",
    "box_to_ellipse_polygon",
    "box_to_rectangle_polygon",
    "boxes_to_masks",
    "format_segmentation_row",
    "list_draft_status",
    "load_draft_status",
    "load_sam_impl",
    "parse_backend",
    "parse_class_map",
    "parse_detection_label",
    "polygon_for_box_sam",
    "promote_drafts",
    "reject_drafts",
    "save_draft_status",
    # Private names retained for tests that patch the package module.
    "_SAM_INSTALL_NOTE",
    "_load_sam_impl",
]
