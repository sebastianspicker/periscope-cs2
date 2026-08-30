"""Self-train label helpers: write policies, QC, conversions, report types."""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS


class SelfTrainError(ValueError):
    """Self-train iteration cannot proceed safely."""


PSEUDO_MARKER = "# pseudo"
_VALID_WRITE_POLICIES = frozenset({"if_absent", "overwrite_pseudo", "overwrite_always"})


@dataclass(frozen=True)
class SelfTrainReport:
    """Counts and paths from one self-train iteration."""

    images_scanned: int
    already_labeled: int
    unlabeled_scanned: int
    accepted: int
    rejected_low_conf: int
    rejected_empty: int
    conf_threshold: float
    labels_written: tuple[str, ...] = field(default_factory=tuple)
    report_path: str | None = None
    student_model: str = ""
    student_manifest: str = ""
    rejected_qc: int = 0
    rejected_teacher: int = 0
    mid_band: int = 0
    overwritten: int = 0

    def as_dict(self) -> dict[str, object]:
        return {
            "images_scanned": self.images_scanned,
            "already_labeled": self.already_labeled,
            "unlabeled_scanned": self.unlabeled_scanned,
            "accepted": self.accepted,
            "rejected_low_conf": self.rejected_low_conf,
            "rejected_empty": self.rejected_empty,
            "rejected_qc": self.rejected_qc,
            "rejected_teacher": self.rejected_teacher,
            "mid_band": self.mid_band,
            "overwritten": self.overwritten,
            "conf_threshold": self.conf_threshold,
            "labels_written": list(self.labels_written),
            "report_path": self.report_path,
            "student_model": self.student_model,
            "student_manifest": self.student_manifest,
            "schema_version": 1,
        }


def _require_dir(path: Path, label: str) -> Path:
    if path.is_symlink():
        raise SelfTrainError(f"{label} must not be a symlink: {path}")
    if not path.is_dir():
        raise SelfTrainError(f"{label} is not a directory: {path}")
    return path


def _require_file(path: Path, label: str) -> Path:
    if path.is_symlink():
        raise SelfTrainError(f"{label} must not be a symlink: {path}")
    if not path.is_file():
        raise SelfTrainError(f"{label} is not a regular file: {path}")
    return path


def _collect_images(images_dir: Path) -> list[Path]:
    files: list[Path] = []
    for path in sorted(images_dir.iterdir()):
        if path.is_symlink():
            continue
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
            files.append(path)
    return files


def instances_to_yolo_seg_lines(
    instances: Sequence[InstanceMask],
    *,
    image_width: int,
    image_height: int,
) -> list[str]:
    """Convert pixel-space InstanceMasks to YOLO-seg label lines.

    Format: ``class_id x1 y1 x2 y2 ... xn yn`` with coordinates normalized to
    ``[0, 1]`` relative to ``image_width`` / ``image_height``.
    """
    if image_width <= 0 or image_height <= 0:
        raise SelfTrainError("image dimensions must be positive")
    lines: list[str] = []
    for inst in instances:
        if len(inst.polygon) < 3:
            continue
        coords: list[str] = []
        for x, y in inst.polygon:
            nx = min(1.0, max(0.0, float(x) / float(image_width)))
            ny = min(1.0, max(0.0, float(y) / float(image_height)))
            coords.append(f"{nx:.6f}")
            coords.append(f"{ny:.6f}")
        lines.append(f"{inst.class_id} " + " ".join(coords))
    return lines


def max_confidence(instances: Sequence[InstanceMask]) -> float | None:
    """Return max confidence among instances, or None if empty."""
    if not instances:
        return None
    return max(float(m.confidence) for m in instances)


def conf_band(max_conf: float, *, conf_low: float, conf_high: float) -> str:
    """Classify confidence into ``low`` / ``mid`` / ``high`` bands."""
    if max_conf < conf_low:
        return "low"
    if max_conf < conf_high:
        return "mid"
    return "high"


def _pseudo_sidecar_path(label_path: Path) -> Path:
    """Sidecar marker path for revisable pseudo labels (YOLO-safe)."""
    return label_path.with_name(label_path.name + ".pseudo")


def _label_is_pseudo_or_empty(label_path: Path) -> bool:
    """True when the label is missing content or marked as pseudo.

    Markers (any):
    - empty / missing label file
    - sidecar ``*.txt.pseudo`` (preferred; Ultralytics-safe)
    - legacy first line ``# pseudo`` in the label body
    """
    if not label_path.exists() or label_path.is_symlink():
        return True
    if _pseudo_sidecar_path(label_path).is_file():
        return True
    try:
        text = label_path.read_text(encoding="utf-8")
    except OSError:
        return False
    if not text.strip():
        return True
    first = text.splitlines()[0].strip()
    return first.startswith(PSEUDO_MARKER)


def _should_skip_existing_label(label_path: Path, policy: str) -> bool:
    """Return True when an existing label should not be reprocessed."""
    if not label_path.exists() or label_path.is_symlink():
        return False
    if policy == "overwrite_always":
        return False
    if policy == "if_absent":
        return True
    if policy == "overwrite_pseudo":
        # Gold (non-empty, non-pseudo) is never reprocessed.
        return not _label_is_pseudo_or_empty(label_path)
    raise SelfTrainError(f"unknown write_policy: {policy}")


def write_label(
    label_path: Path,
    lines: Sequence[str],
    *,
    policy: str = "if_absent",
) -> bool:
    """Write a YOLO label according to ``policy``.

    Policies
    --------
    ``if_absent``:
        Never overwrite an existing file (careful merge).
    ``overwrite_pseudo``:
        Write when missing, empty, or marked pseudo (sidecar or legacy
        ``# pseudo`` header). Never overwrite gold labels.
    ``overwrite_always``:
        Always write (dangerous; intended for tests).

    Pseudo labels use a sibling ``*.txt.pseudo`` sidecar so the ``.txt`` body
    stays pure YOLO-seg (Ultralytics does not accept comment lines). Legacy
    ``# pseudo`` headers are still recognized when deciding overwrite.

    Returns True when a file was written; False when skipped by policy.
    """
    if policy not in _VALID_WRITE_POLICIES:
        raise SelfTrainError(
            f"write_policy must be one of {sorted(_VALID_WRITE_POLICIES)}, got {policy!r}"
        )
    if label_path.is_symlink():
        raise SelfTrainError(f"label path must not be a symlink: {label_path}")

    if label_path.exists():
        if policy == "if_absent":
            return False
        if policy == "overwrite_pseudo" and not _label_is_pseudo_or_empty(label_path):
            return False
        # overwrite_always or revisable pseudo/empty falls through

    label_path.parent.mkdir(parents=True, exist_ok=True)
    # Strip comment lines so training always sees pure YOLO tokens.
    clean_lines = [ln for ln in lines if ln.strip() and not ln.strip().startswith("#")]
    body = "\n".join(clean_lines)
    payload = (body + "\n") if body else ""
    tmp = label_path.with_suffix(label_path.suffix + ".tmp")
    sidecar = _pseudo_sidecar_path(label_path)
    try:
        tmp.write_text(payload, encoding="utf-8")
        tmp.replace(label_path)
        # Mark as revisable pseudo (not gold).
        sidecar.write_text("pseudo\n", encoding="utf-8")
    except OSError as error:
        tmp.unlink(missing_ok=True)
        raise SelfTrainError(f"could not write label {label_path}: {error}") from error
    return True


def write_label_if_absent(label_path: Path, lines: Sequence[str]) -> bool:
    """Atomically write a YOLO label if the path does not already exist.

    Returns True when a new file was written; False when skipped because the
    destination already exists (careful merge — never overwrite).
    """
    return write_label(label_path, lines, policy="if_absent")


def qc_yolo_seg_lines(
    lines: Sequence[str],
    *,
    allowed_class_ids: set[int] | None = None,
    min_points: int = 3,
    min_area: float = 1e-6,
) -> list[str]:
    """Drop invalid / tiny / disallowed YOLO-seg (or det) label lines.

    Detection format (5 tokens: ``cls xc yc w h``) is expanded to a 4-corner
    polygon when the normalized box area is acceptable.
    """
    if min_points < 1:
        raise SelfTrainError("min_points must be >= 1")
    if min_area < 0.0:
        raise SelfTrainError("min_area must be >= 0")

    cleaned: list[str] = []
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        try:
            class_id = int(float(parts[0]))
        except (TypeError, ValueError):
            continue
        if class_id < 0:
            continue
        if allowed_class_ids is not None and class_id not in allowed_class_ids:
            continue

        if len(parts) == 5:
            # YOLO det: class xc yc w h (normalized) → 4-corner poly.
            try:
                xc = float(parts[1])
                yc = float(parts[2])
                bw = float(parts[3])
                bh = float(parts[4])
            except (TypeError, ValueError):
                continue
            if not all(map(np.isfinite, (xc, yc, bw, bh))):
                continue
            if bw <= 0.0 or bh <= 0.0:
                continue
            area = bw * bh
            if area < min_area:
                continue
            x1 = xc - bw / 2.0
            y1 = yc - bh / 2.0
            x2 = xc + bw / 2.0
            y2 = yc + bh / 2.0
            poly_coords = [
                x1,
                y1,
                x2,
                y1,
                x2,
                y2,
                x1,
                y2,
            ]
            if min_points > 4:
                continue
            coord_str = " ".join(f"{c:.6f}" for c in poly_coords)
            cleaned.append(f"{class_id} {coord_str}")
            continue

        # YOLO-seg: class + x1 y1 x2 y2 ...
        coords = parts[1:]
        if len(coords) % 2 != 0:
            continue
        n_points = len(coords) // 2
        if n_points < min_points:
            continue
        try:
            values = [float(c) for c in coords]
        except (TypeError, ValueError):
            continue
        if not all(map(np.isfinite, values)):
            continue
        xs = values[0::2]
        ys = values[1::2]
        area = (max(xs) - min(xs)) * (max(ys) - min(ys))
        if area < min_area:
            continue
        coord_str = " ".join(f"{c:.6f}" for c in values)
        cleaned.append(f"{class_id} {coord_str}")
    return cleaned
