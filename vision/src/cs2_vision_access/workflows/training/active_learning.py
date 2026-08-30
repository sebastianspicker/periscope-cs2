"""Active-learning queue: rank frames by student uncertainty / teacher disagreement.

Scores unlabeled (or candidate) frames so a human review queue prioritizes
hard cases: low max confidence, empty predictions, or high teacher–student
disagreement (count mismatch or low mean matched box IoU).
"""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import cast

from cs2_vision_access.application.ports.segmentation import Segmenter
from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.workflows.training.multi_teacher import (
    Box,
    box_from_polygon,
    box_iou,
)

PredictFn = Callable[[Path], Sequence[InstanceMask]]


@dataclass(frozen=True)
class UncertainFrame:
    """One ranked frame for human review."""

    image_path: str
    stem: str
    score: float
    max_student_conf: float | None
    mean_matched_iou: float | None
    teacher_count: int
    student_count: int
    reason: str

    def as_dict(self) -> dict[str, object]:
        return {
            "image_path": self.image_path,
            "stem": self.stem,
            "score": self.score,
            "max_student_conf": self.max_student_conf,
            "mean_matched_iou": self.mean_matched_iou,
            "teacher_count": self.teacher_count,
            "student_count": self.student_count,
            "reason": self.reason,
        }


def _collect_images(images_dir: Path) -> list[Path]:
    if images_dir.is_symlink() or not images_dir.is_dir():
        raise ValueError(f"images_dir must be a real directory: {images_dir}")
    files: list[Path] = []
    for path in sorted(images_dir.rglob("*")):
        if path.is_symlink():
            continue
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            files.append(path)
    return files


def _masks_to_boxes(masks: Sequence[InstanceMask]) -> list[Box]:
    return [
        box_from_polygon(m.polygon, class_id=m.class_id, confidence=m.confidence) for m in masks
    ]


def _mean_matched_iou(
    teacher_boxes: Sequence[Box],
    student_boxes: Sequence[Box],
    *,
    iou_thresh: float = 0.1,
) -> float | None:
    """Mean IoU of greedy one-to-one matches; None if either side is empty."""
    if not teacher_boxes or not student_boxes:
        return None
    # Greedy match by descending IoU (same class).
    candidates: list[tuple[float, int, int]] = []
    for ti, tb in enumerate(teacher_boxes):
        for si, sb in enumerate(student_boxes):
            if tb.class_id != sb.class_id:
                continue
            iou = box_iou(tb, sb)
            if iou >= iou_thresh:
                candidates.append((iou, ti, si))
    candidates.sort(key=lambda item: (-item[0], item[1], item[2]))
    used_t: set[int] = set()
    used_s: set[int] = set()
    ious: list[float] = []
    for iou, ti, si in candidates:
        if ti in used_t or si in used_s:
            continue
        used_t.add(ti)
        used_s.add(si)
        ious.append(iou)
    if not ious:
        return 0.0
    return sum(ious) / len(ious)


def score_uncertainty(
    teacher_masks: Sequence[InstanceMask],
    student_masks: Sequence[InstanceMask],
    *,
    iou_thresh: float = 0.1,
) -> tuple[float, str, float | None, float | None]:
    """Compute uncertainty score in [0, 1] (higher = more uncertain).

    Components
    ----------
    * low max student confidence → ``1 - max_conf`` (empty → 1.0)
    * count mismatch between teacher and student → 1.0
    * low mean matched IoU → ``1 - mean_iou``

    Final score is the max of the active components.
    """
    max_conf: float | None
    if student_masks:
        max_conf = max(float(m.confidence) for m in student_masks)
        low_conf_score = 1.0 - max_conf
    else:
        max_conf = None
        low_conf_score = 1.0

    t_boxes = _masks_to_boxes(teacher_masks)
    s_boxes = _masks_to_boxes(student_masks)
    mean_iou = _mean_matched_iou(t_boxes, s_boxes, iou_thresh=iou_thresh)

    disagreement_score = 0.0
    reasons: list[str] = []

    if len(teacher_masks) != len(student_masks):
        disagreement_score = max(disagreement_score, 1.0)
        reasons.append("count_mismatch")
    if mean_iou is not None:
        disagreement_score = max(disagreement_score, 1.0 - mean_iou)
        if mean_iou < 0.5:
            reasons.append("low_iou")
    elif teacher_masks and not student_masks:
        disagreement_score = 1.0
        reasons.append("student_empty")
    elif student_masks and not teacher_masks:
        disagreement_score = 1.0
        reasons.append("teacher_empty")

    if max_conf is None:
        reasons.append("no_student_pred")
    elif max_conf < 0.5:
        reasons.append("low_conf")

    score = max(low_conf_score, disagreement_score)
    # Clamp numerical noise.
    score = min(1.0, max(0.0, float(score)))
    reason = "+".join(reasons) if reasons else "confident"
    return score, reason, max_conf, mean_iou


def rank_uncertain_frames(
    images_dir: str | Path,
    teacher_predict_fn: PredictFn,
    student_predict_fn: PredictFn,
    *,
    top_k: int = 50,
    queue_path: str | Path | None = None,
    iou_thresh: float = 0.1,
) -> list[UncertainFrame]:
    """Rank frames by uncertainty for active-learning review.

    Parameters
    ----------
    images_dir:
        Directory of candidate images (walked recursively).
    teacher_predict_fn / student_predict_fn:
        Callables ``(image_path: Path) -> Sequence[InstanceMask]``. Tests pass
        pure mocks; production wires segmenters.
    top_k:
        Return at most this many highest-scoring frames (0 = all).
    queue_path:
        Optional JSONL path. When set, write one JSON object per ranked frame.
    iou_thresh:
        Minimum IoU for teacher/student box matching when scoring disagreement.
    """
    if isinstance(top_k, bool) or not isinstance(top_k, int) or top_k < 0:
        raise ValueError("top_k must be an integer >= 0")

    root = Path(images_dir)
    images = _collect_images(root)
    ranked: list[UncertainFrame] = []

    for image_path in images:
        teacher = tuple(teacher_predict_fn(image_path))
        student = tuple(student_predict_fn(image_path))
        score, reason, max_conf, mean_iou = score_uncertainty(
            teacher, student, iou_thresh=iou_thresh
        )
        ranked.append(
            UncertainFrame(
                image_path=image_path.as_posix(),
                stem=image_path.stem,
                score=score,
                max_student_conf=max_conf,
                mean_matched_iou=mean_iou,
                teacher_count=len(teacher),
                student_count=len(student),
                reason=reason,
            )
        )

    ranked.sort(key=lambda item: (-item.score, item.stem))
    if top_k > 0:
        ranked = ranked[:top_k]

    if queue_path is not None:
        out = Path(queue_path)
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open("w", encoding="utf-8") as handle:
            for item in ranked:
                handle.write(json.dumps(item.as_dict(), sort_keys=True) + "\n")

    return ranked


def rank_uncertain_queue(
    queue_path: str | Path,
    out_path: str | Path | None = None,
    *,
    top_k: int = 50,
    out_jsonl: str | Path | None = None,
) -> list[dict[str, object]]:
    """Rank a self-train uncertain_queue JSONL by ascending max_conf.

    Lightweight alternative to :func:`rank_uncertain_frames` that needs no
    model: each queue line is expected to contain ``stem``, ``max_conf``, and
    optionally ``path``. Uncertainty score is ``1 - max_conf`` (missing conf
    sorts as most uncertain).

    Parameters
    ----------
    queue_path:
        Input JSONL produced by self-train (``uncertain_queue.jsonl``).
    out_path:
        Optional JSON list path (e.g. ``progress/uncertain_review.json``).
    top_k:
        Keep at most this many frames (0 = all).
    out_jsonl:
        Optional JSONL path for the same ranked rows.

    Returns
    -------
    Ranked list of dicts (highest uncertainty first).
    """
    if isinstance(top_k, bool) or not isinstance(top_k, int) or top_k < 0:
        raise ValueError("top_k must be an integer >= 0")

    src = Path(queue_path)
    if not src.is_file():
        return []

    rows: list[dict[str, object]] = []
    with src.open(encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if not isinstance(record, dict):
                continue
            conf_raw = record.get("max_conf")
            try:
                max_conf = float(conf_raw) if conf_raw is not None else None
            except (TypeError, ValueError):
                max_conf = None
            stem = record.get("stem")
            if stem is None and record.get("path") is not None:
                stem = Path(str(record["path"])).stem
            stem_s = str(stem) if stem is not None else f"row_{line_no}"
            score = 1.0 if max_conf is None else max(0.0, min(1.0, 1.0 - max_conf))
            entry: dict[str, object] = {
                "stem": stem_s,
                "max_conf": max_conf,
                "score": score,
                "path": record.get("path"),
                "reason": ("low_conf" if (max_conf is None or max_conf < 0.5) else "mid_band"),
            }
            for key, value in record.items():
                if key not in entry:
                    entry[key] = value
            rows.append(entry)

    # Deduplicate by stem, keep lowest conf (highest score).
    best: dict[str, dict[str, object]] = {}
    for row in rows:
        stem_key = str(row["stem"])
        prev = best.get(stem_key)
        if prev is None or float(row["score"]) > float(prev["score"]):  # type: ignore[arg-type]
            best[stem_key] = row
    ranked = sorted(
        best.values(),
        key=lambda item: (-float(item["score"]), str(item["stem"])),  # type: ignore[arg-type]
    )
    if top_k > 0:
        ranked = ranked[:top_k]

    if out_path is not None:
        dest = Path(out_path)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(
            json.dumps(ranked, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    if out_jsonl is not None:
        dest_l = Path(out_jsonl)
        dest_l.parent.mkdir(parents=True, exist_ok=True)
        with dest_l.open("w", encoding="utf-8") as handle:
            for item in ranked:
                handle.write(json.dumps(item, sort_keys=True) + "\n")

    return ranked


def rank_uncertain_queue_soft(
    queue_path: Path | str,
    progress_dir: Path | str,
    *,
    top_k: int = 50,
) -> Path | None:
    """Best-effort rank queue to ``progress_dir/uncertain_review.json(.jsonl)``.

    Never raises. Returns the path to ``uncertain_review.json`` on success,
    or None when the queue is missing/empty or ranking fails.
    """
    try:
        src = Path(queue_path)
        if not src.is_file() or src.stat().st_size == 0:
            return None
        out_dir = Path(progress_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        out_json = out_dir / "uncertain_review.json"
        out_jsonl = out_dir / "uncertain_review.jsonl"
        rank_uncertain_queue(
            src,
            out_json,
            top_k=top_k,
            out_jsonl=out_jsonl,
        )
        return out_json
    except Exception:  # noqa: BLE001 — soft; never break self-train callers
        return None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="python -m cs2_vision_access.workflows.training.active_learning",
        description=(
            "Rank frames by student uncertainty / teacher–student disagreement "
            "for an active-learning review queue."
        ),
    )
    p.add_argument(
        "--images-dir",
        type=Path,
        help="Directory of candidate images (required for a real run)",
    )
    p.add_argument(
        "--top-k",
        type=int,
        default=50,
        help="Maximum frames to keep in the queue (default: 50; 0 = all)",
    )
    p.add_argument(
        "--queue",
        type=Path,
        default=None,
        help="Optional JSONL output path for the ranked queue",
    )
    p.add_argument(
        "--teacher-model",
        type=Path,
        default=None,
        help="Optional teacher ONNX model (with --teacher-manifest)",
    )
    p.add_argument(
        "--teacher-manifest",
        type=Path,
        default=None,
        help="Teacher model manifest JSON",
    )
    p.add_argument(
        "--student-model",
        type=Path,
        default=None,
        help="Optional student ONNX model (with --student-manifest)",
    )
    p.add_argument(
        "--student-manifest",
        type=Path,
        default=None,
        help="Student model manifest JSON",
    )
    p.add_argument(
        "--device",
        default="cpu",
        help="Inference device (default: cpu)",
    )
    return p


def main(argv: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    # --help is handled by argparse. Without models, print usage guidance.
    if args.images_dir is None:
        parser.print_help()
        print(
            "\nNote: wire --images-dir plus teacher/student ONNX+manifest paths "
            "for a live ranking run, or call rank_uncertain_frames() in Python "
            "with custom predict callables."
        )
        return 0

    if not args.images_dir.is_dir():
        parser.error(f"images dir not found: {args.images_dir}")

    need = (
        args.teacher_model,
        args.teacher_manifest,
        args.student_model,
        args.student_manifest,
    )
    if any(need) and not all(need):
        parser.error(
            "all of --teacher-model, --teacher-manifest, --student-model, "
            "and --student-manifest are required together"
        )
    if not all(need):
        parser.error(
            "live ranking requires --teacher-model/--teacher-manifest and "
            "--student-model/--student-manifest (or use the Python API)"
        )

    import cv2

    from cs2_vision_access.application.ports.segmentation import create_segmenter

    teacher = create_segmenter(args.teacher_model, args.teacher_manifest, device=args.device)
    student = create_segmenter(args.student_model, args.student_manifest, device=args.device)

    def _predict_with(seg: object) -> PredictFn:
        def _fn(image_path: Path) -> tuple[InstanceMask, ...]:
            frame = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
            if frame is None:
                return ()
            return tuple(cast(Segmenter, seg).predict(frame, frame_index=0))

        return _fn

    ranked = rank_uncertain_frames(
        args.images_dir,
        _predict_with(teacher),
        _predict_with(student),
        top_k=args.top_k,
        queue_path=args.queue,
    )
    print(f"Ranked {len(ranked)} frames (top_k={args.top_k})")
    for item in ranked[:10]:
        print(
            f"  {item.score:.3f}  {item.stem}  "
            f"t={item.teacher_count} s={item.student_count}  {item.reason}"
        )
    if args.queue is not None:
        print(f"Queue written to {args.queue}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


__all__ = [
    "PredictFn",
    "UncertainFrame",
    "rank_uncertain_frames",
    "rank_uncertain_queue",
    "rank_uncertain_queue_soft",
    "score_uncertainty",
    "main",
]
