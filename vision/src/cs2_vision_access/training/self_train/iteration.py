"""One self-training iteration: pseudo-label unlabeled frames with a student."""

from __future__ import annotations

import json
from collections.abc import Sequence
from pathlib import Path

import cv2
import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.segmenters import create_segmenter
from cs2_vision_access.training.multi_teacher import box_from_polygon, box_iou
from cs2_vision_access.training.self_train.labels import (
    _VALID_WRITE_POLICIES,
    SelfTrainError,
    SelfTrainReport,
    _collect_images,
    _require_dir,
    _require_file,
    _should_skip_existing_label,
    instances_to_yolo_seg_lines,
    max_confidence,
    qc_yolo_seg_lines,
    write_label,
)


def _append_uncertain_queue(
    queue_path: Path,
    *,
    stem: str,
    max_conf: float,
    path: Path,
) -> None:
    queue_path.parent.mkdir(parents=True, exist_ok=True)
    record = {
        "stem": stem,
        "max_conf": float(max_conf),
        "path": path.as_posix(),
    }
    with queue_path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, sort_keys=True) + "\n")


def _filter_by_teacher(
    student: Sequence[InstanceMask],
    teacher: Sequence[InstanceMask],
    *,
    min_iou: float,
) -> list[InstanceMask]:
    """Keep student instances that match a same-class teacher box by IoU."""
    if not teacher:
        return []
    teacher_boxes = [
        box_from_polygon(t.polygon, class_id=t.class_id, confidence=t.confidence)
        for t in teacher
        if len(t.polygon) >= 1
    ]
    kept: list[InstanceMask] = []
    for s in student:
        if len(s.polygon) < 1:
            continue
        s_box = box_from_polygon(s.polygon, class_id=s.class_id, confidence=s.confidence)
        for t_box in teacher_boxes:
            if t_box.class_id != s_box.class_id:
                continue
            if box_iou(s_box, t_box) >= min_iou:
                kept.append(s)
                break
    return kept


def run_self_train_iteration(
    images_dir: str | Path,
    labels_dir: str | Path,
    student_model: str | Path,
    student_manifest: str | Path,
    *,
    conf_threshold: float = 0.5,
    device: str = "cpu",
    max_frames: int = 0,
    report_path: str | Path | None = None,
    segmenter=None,
    write_policy: str = "if_absent",
    allowed_class_ids: set[int] | None = None,
    min_polygon_area: float = 1e-6,
    teacher_model: str | Path | None = None,
    teacher_manifest: str | Path | None = None,
    teacher_segmenter=None,
    teacher_min_iou: float = 0.3,
    teacher_allow_student_only: bool = False,
    conf_high: float | None = None,
    conf_low: float | None = None,
    uncertain_queue_path: Path | None = None,
    fail_closed: bool = False,
) -> SelfTrainReport:
    """Run one self-training pseudo-label iteration.

    Parameters
    ----------
    images_dir / labels_dir:
        Flat YOLO-style image and label directories (sibling stems).
    student_model / student_manifest:
        Student ONNX weights and trust manifest (used when ``segmenter`` is
        None).
    conf_threshold:
        Keep a frame only when the max instance confidence is ≥ this value
        (when conf bands are not used for the high gate).
    device:
        Inference device string passed to :func:`create_segmenter`.
    max_frames:
        Cap on unlabeled frames to process (0 = unlimited).
    report_path:
        Optional JSON path for the report. Defaults to
        ``labels_dir / self_train_report.json``.
    segmenter:
        Optional pre-built segmenter (for tests). Must implement
        ``predict(frame_bgr, *, frame_index) -> Sequence[InstanceMask]``.
    write_policy:
        Label write policy: ``if_absent``, ``overwrite_pseudo``, or
        ``overwrite_always``.
    allowed_class_ids / min_polygon_area:
        Label QC filters applied before writing.
    teacher_model / teacher_manifest / teacher_segmenter:
        Optional teacher gate; student instances must match a same-class
        teacher detection by box IoU ≥ ``teacher_min_iou``.
    teacher_allow_student_only:
        When False (default), empty teacher predictions reject the frame.
    conf_high / conf_low:
        Optional confidence bands. High band auto-accepts; mid band is
        queued (not written); low band counts as low-conf reject.
    uncertain_queue_path:
        Optional JSONL path for mid-band frames.
    fail_closed:
        Raise if zero images are found or the segmenter hard-fails on every
        scanned unlabeled frame.
    """
    if not 0.0 <= float(conf_threshold) <= 1.0:
        raise SelfTrainError("conf_threshold must be in [0, 1]")
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames < 0:
        raise SelfTrainError("max_frames must be an integer >= 0")
    if write_policy not in _VALID_WRITE_POLICIES:
        raise SelfTrainError(
            f"write_policy must be one of {sorted(_VALID_WRITE_POLICIES)}, got {write_policy!r}"
        )
    if conf_high is not None and not 0.0 <= float(conf_high) <= 1.0:
        raise SelfTrainError("conf_high must be in [0, 1]")
    if conf_low is not None and not 0.0 <= float(conf_low) <= 1.0:
        raise SelfTrainError("conf_low must be in [0, 1]")
    if conf_low is not None and conf_high is not None and float(conf_low) > float(conf_high):
        raise SelfTrainError("conf_low must be <= conf_high")
    if not 0.0 <= float(teacher_min_iou) <= 1.0:
        raise SelfTrainError("teacher_min_iou must be in [0, 1]")

    images_root = _require_dir(Path(images_dir), "images_dir")
    labels_root = Path(labels_dir)
    if labels_root.is_symlink():
        raise SelfTrainError(f"labels_dir must not be a symlink: {labels_root}")
    labels_root.mkdir(parents=True, exist_ok=True)
    if not labels_root.is_dir():
        raise SelfTrainError(f"labels_dir is not a directory: {labels_root}")

    model_path = Path(student_model)
    manifest_path = Path(student_manifest)
    if segmenter is None:
        _require_file(model_path, "student_model")
        _require_file(manifest_path, "student_manifest")
        # Low floor so weak detections still reach our conf_threshold filter.
        segmenter = create_segmenter(
            model_path,
            manifest_path,
            device=device,
            confidence=0.05,
        )

    teacher = teacher_segmenter
    if teacher is None and teacher_model is not None:
        t_model = Path(teacher_model)
        t_manifest = Path(teacher_manifest) if teacher_manifest is not None else None
        _require_file(t_model, "teacher_model")
        if t_manifest is None:
            raise SelfTrainError("teacher_manifest is required when teacher_model is set")
        _require_file(t_manifest, "teacher_manifest")
        teacher = create_segmenter(
            t_model,
            t_manifest,
            device=device,
            confidence=0.05,
        )

    images = _collect_images(images_root)
    if fail_closed and not images:
        raise SelfTrainError(f"fail_closed: no images found in {images_root}")

    already_labeled = 0
    unlabeled_scanned = 0
    accepted = 0
    rejected_low_conf = 0
    rejected_empty = 0
    rejected_qc = 0
    rejected_teacher = 0
    mid_band = 0
    overwritten = 0
    hard_fails = 0
    written: list[str] = []

    for image_path in images:
        label_path = labels_root / f"{image_path.stem}.txt"
        if _should_skip_existing_label(label_path, write_policy):
            already_labeled += 1
            continue

        if max_frames > 0 and unlabeled_scanned >= max_frames:
            break

        unlabeled_scanned += 1
        existed_before = label_path.exists() and not label_path.is_symlink()

        frame = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
        if frame is None:
            rejected_empty += 1
            continue
        if not isinstance(frame, np.ndarray) or frame.ndim != 3:
            rejected_empty += 1
            continue

        height, width = frame.shape[:2]
        try:
            predictions = tuple(segmenter.predict(frame, frame_index=0))
        except Exception:
            # Fail closed on backend errors for this frame — do not write labels.
            hard_fails += 1
            rejected_empty += 1
            continue

        if not predictions:
            rejected_empty += 1
            continue

        max_conf = max_confidence(predictions)
        if max_conf is None:
            rejected_empty += 1
            continue

        # Confidence bands (optional) + legacy conf_threshold gate.
        if conf_low is not None and max_conf < float(conf_low):
            rejected_low_conf += 1
            continue
        if conf_high is not None and max_conf < float(conf_high):
            mid_band += 1
            if uncertain_queue_path is not None:
                _append_uncertain_queue(
                    Path(uncertain_queue_path),
                    stem=image_path.stem,
                    max_conf=max_conf,
                    path=image_path,
                )
            continue
        if conf_high is None and max_conf < conf_threshold:
            rejected_low_conf += 1
            continue

        # Keep only instances that themselves meet the threshold.
        kept = tuple(m for m in predictions if m.confidence >= conf_threshold)
        if not kept:
            rejected_low_conf += 1
            continue

        # Optional teacher gate: require same-class box IoU agreement.
        if teacher is not None:
            try:
                teacher_preds = tuple(teacher.predict(frame, frame_index=0))
            except Exception:
                hard_fails += 1
                rejected_teacher += 1
                continue
            if not teacher_preds:
                if not teacher_allow_student_only:
                    rejected_teacher += 1
                    continue
            else:
                matched = _filter_by_teacher(kept, teacher_preds, min_iou=float(teacher_min_iou))
                if not matched:
                    rejected_teacher += 1
                    continue
                kept = tuple(matched)

        lines = instances_to_yolo_seg_lines(kept, image_width=width, image_height=height)
        lines = qc_yolo_seg_lines(
            lines,
            allowed_class_ids=allowed_class_ids,
            min_area=float(min_polygon_area),
        )
        if not lines:
            rejected_qc += 1
            continue

        if write_label(label_path, lines, policy=write_policy):
            accepted += 1
            written.append(label_path.as_posix())
            if existed_before:
                overwritten += 1
        else:
            # Race / non-revisable gold: count as already labeled, not accepted.
            already_labeled += 1

    if fail_closed and unlabeled_scanned > 0 and hard_fails == unlabeled_scanned:
        raise SelfTrainError("fail_closed: segmenter hard-failed on every unlabeled frame")

    out_report = (
        Path(report_path) if report_path is not None else (labels_root / "self_train_report.json")
    )
    report = SelfTrainReport(
        images_scanned=len(images),
        already_labeled=already_labeled,
        unlabeled_scanned=unlabeled_scanned,
        accepted=accepted,
        rejected_low_conf=rejected_low_conf,
        rejected_empty=rejected_empty,
        conf_threshold=float(conf_threshold),
        labels_written=tuple(written),
        report_path=out_report.as_posix(),
        student_model=model_path.as_posix(),
        student_manifest=manifest_path.as_posix(),
        rejected_qc=rejected_qc,
        rejected_teacher=rejected_teacher,
        mid_band=mid_band,
        overwritten=overwritten,
    )
    try:
        out_report.parent.mkdir(parents=True, exist_ok=True)
        out_report.write_text(
            json.dumps(report.as_dict(), indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except OSError as error:
        raise SelfTrainError(f"could not write report {out_report}: {error}") from error
    return report
