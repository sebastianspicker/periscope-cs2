"""One self-training iteration: pseudo-label unlabeled frames with a student.

Orchestration (labeling/prepare is external):

1. Scan ``images_dir`` for frames that do **not** yet have a sibling label
   under ``labels_dir`` (or that have revisable pseudo labels).
2. Run the student ONNX segmenter on each unlabeled frame.
3. Accept the prediction as a YOLO-seg label only when
   ``max(confidence) >= conf_threshold`` (or conf-band / teacher gates)
   and at least one instance exists.
4. Merge carefully according to ``write_policy``; never overwrite gold by
   default. Pseudo labels use a ``*.txt.pseudo`` sidecar (legacy ``# pseudo``
   headers are still recognized) so later iterations can revise them.
5. Write a JSON report with counts.
"""

from __future__ import annotations

# Re-export patch targets used by tests.
import cv2 as cv2

from cs2_vision_access.training.self_train.iteration import run_self_train_iteration
from cs2_vision_access.training.self_train.labels import (
    PSEUDO_MARKER,
    SelfTrainError,
    SelfTrainReport,
    _label_is_pseudo_or_empty,
    conf_band,
    instances_to_yolo_seg_lines,
    max_confidence,
    qc_yolo_seg_lines,
    write_label,
    write_label_if_absent,
)

__all__ = [
    "PSEUDO_MARKER",
    "SelfTrainError",
    "SelfTrainReport",
    "conf_band",
    "instances_to_yolo_seg_lines",
    "max_confidence",
    "qc_yolo_seg_lines",
    "run_self_train_iteration",
    "write_label",
    "write_label_if_absent",
    "_label_is_pseudo_or_empty",
]
