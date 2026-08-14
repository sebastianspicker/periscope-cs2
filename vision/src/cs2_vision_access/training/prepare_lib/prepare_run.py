"""Prepare run API: label video/images with Vombit+EdgeSAM."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path

import cv2

from cs2_vision_access.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.segmenters.cs2_sam import Cs2SamSegmenter
from cs2_vision_access.training.contracts import VOMBIT_TO_PLAYER
from cs2_vision_access.training.sources import (
    _from_video,
    _polygon_area_px,
    _process_frame,
    _write_dataset_yaml,
    ensure_image_label_dirs,
    next_frame_index,
    resolve_write_root,
)


@dataclass
class PrepareResult:
    """Outcome of a prepare run."""

    labeled_frames: int
    write_root: str
    notes: list[str] = field(default_factory=list)


def run_cs2_sam_prepare(
    *,
    detector: str | Path,
    manifest: str | Path,
    encoder: str | Path,
    decoder: str | Path,
    output_dir: str | Path,
    video: str | Path | None = None,
    images_dir: str | Path | None = None,
    labels_dir: str | Path | None = None,
    session_id: str | None = None,
    device: str = "cpu",
    confidence: float = 0.4,
    sample_rate: float = 3.0,
    max_frames: int = 0,
    max_players: int = 0,
    collapse_to_player: bool = True,
    keep_negatives: bool = True,
    negative_every_n: int = 5,
    min_mask_area: float = 0.0,
    min_confidence: float | None = None,
    write_yaml: bool = True,
    class_names: Mapping[int, str] | None = None,
) -> PrepareResult:
    """Run Vombit+EdgeSAM prepare into a flat images/labels dataset.

    Exactly one of *video* or *images_dir* must be set.

    * **video** — sample frames, write ``frame_XXXXXXXX.jpg`` + labels under
      ``resolve_write_root(output_dir, session_id)``.
    * **images_dir** — label existing images in place; only missing or empty
      label files are filled (non-empty labels are left unchanged). Labels are
      written to *labels_dir* when set, else a sibling ``labels/`` (or
      ``labels/<split>`` for ``images/<split>`` session layouts).

    Args:
        detector / manifest / encoder / decoder: Model asset paths (must exist).
        output_dir: Dataset root (or parent of session staging).
        video: Gameplay video path.
        images_dir: Directory of existing frames to label.
        session_id: Optional staging id under *output_dir* (video mode).
        device: ONNX Runtime device string.
        confidence: Detector confidence threshold.
        sample_rate: Video frames per second to sample.
        max_frames: Cap on labeled frames written (0 = unlimited).
        max_players: Cap detections per frame (0 = unlimited).
        collapse_to_player: Map Vombit classes via :data:`VOMBIT_TO_PLAYER`.
        keep_negatives: Write empty-label negatives when no instances remain.
        negative_every_n: Keep every Nth empty attempt when *keep_negatives*.
        min_mask_area: Drop instances with polygon area (px) below this.
        min_confidence: Optional post-predict confidence floor.
        write_yaml: Write ``dataset.yaml`` under the write root.
        class_names: Optional class name map for yaml (overrides collapse default).

    Returns:
        :class:`PrepareResult` with counts and write root.

    Raises:
        FileNotFoundError: Required path missing.
        ValueError: Invalid source combination or empty paths.
        RuntimeError: Video cannot be opened (from ``_from_video``).
    """
    notes: list[str] = []
    detector_p = _require_file(Path(detector), "detector")
    manifest_p = _require_file(Path(manifest), "manifest")
    encoder_p = _require_file(Path(encoder), "encoder")
    decoder_p = _require_file(Path(decoder), "decoder")

    has_video = video is not None
    has_images = images_dir is not None
    if has_video == has_images:
        raise ValueError("exactly one of video or images_dir must be specified")

    class_map: dict[int, int] | None = dict(VOMBIT_TO_PLAYER) if collapse_to_player else None

    segmenter = Cs2SamSegmenter(
        detector_p,
        manifest_p,
        class_names=("ct", "ct_head", "t", "t_head"),
        confidence=confidence,
        image_size=640,
        device=device,
        sam_encoder_path=encoder_p,
        sam_decoder_path=decoder_p,
    )

    process_kw = dict(
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        class_map=class_map,
        min_confidence=min_confidence,
        min_mask_area=min_mask_area,
    )

    if has_video:
        video_p = _require_file(Path(video), "video")  # type: ignore[arg-type]
        write_root = resolve_write_root(Path(output_dir), session_id)
        ensure_image_label_dirs(write_root)
        start_index = next_frame_index(write_root)
        if start_index > 0:
            notes.append(f"resuming at frame index {start_index}")
        labeled = _from_video(
            str(video_p),
            segmenter,
            write_root,
            sample_rate,
            max_frames,
            max_players,
            start_index=start_index,
            **process_kw,
        )
    else:
        images_path = Path(images_dir)  # type: ignore[arg-type]
        if not images_path.is_dir():
            raise FileNotFoundError(f"images_dir not found: {images_path}")
        write_root, resolved_labels = _resolve_images_layout(
            images_path, Path(output_dir), session_id, labels_dir=labels_dir
        )
        resolved_labels.mkdir(parents=True, exist_ok=True)
        labeled = _from_images_dir(
            images_path,
            resolved_labels,
            write_root,
            segmenter,
            max_frames=max_frames,
            max_players=max_players,
            **process_kw,
        )
        notes.append(f"labeled existing images under {images_path}")

    if write_yaml:
        yaml_classes = dict(class_names) if class_names is not None else None
        _write_dataset_yaml(write_root, classes=yaml_classes, class_map=class_map)
        notes.append(f"wrote dataset.yaml under {write_root}")

    return PrepareResult(
        labeled_frames=labeled,
        write_root=str(write_root),
        notes=notes,
    )


def _require_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path


def _resolve_images_layout(
    images_path: Path,
    output_dir: Path,
    session_id: str | None,
    *,
    labels_dir: str | Path | None = None,
) -> tuple[Path, Path]:
    """Return ``(write_root, labels_dir)`` for an existing images directory.

    Handles flat ``images/``, session ``images/<split>/``, CS2-10k
    ``images_val/``, and explicit *labels_dir* overrides.
    """
    if labels_dir is not None:
        lbl = Path(labels_dir)
        if images_path.name == "images":
            write_root = images_path.parent
        elif images_path.name in {"train", "val", "test"} and images_path.parent.name == "images":
            write_root = images_path.parent.parent
        else:
            write_root = resolve_write_root(output_dir, session_id)
        return write_root, lbl

    if images_path.name == "images":
        write_root = images_path.parent
        return write_root, write_root / "labels"

    # session_split: images/train → labels/train under dataset root
    if images_path.name in {"train", "val", "test"} and images_path.parent.name == "images":
        write_root = images_path.parent.parent
        return write_root, write_root / "labels" / images_path.name

    # CS2-10k holdout staging: images_val/ → flat labels/ under parent.
    if images_path.name == "images_val":
        write_root = images_path.parent
        return write_root, write_root / "labels"

    # Non-standard: labels sibling of images_dir; yaml under output/session.
    write_root = resolve_write_root(output_dir, session_id)
    return write_root, images_path.parent / "labels"


def _from_images_dir(
    images_path: Path,
    labels_dir: Path,
    write_root: Path,
    segmenter: Cs2SamSegmenter,
    *,
    max_frames: int,
    max_players: int,
    keep_negatives: bool = True,
    negative_every_n: int = 5,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> int:
    """Label existing images that lack a non-empty label file.

    Uses the same stem for ``labels/{stem}.txt`` and does not rewrite images.
    When images live under a standard ``images/`` folder, delegates to
    :func:`_process_frame` with ``write_image=False`` so filter logic stays
    shared with video prepare.
    """
    image_files = sorted(
        p
        for p in images_path.iterdir()
        if p.is_file() and not p.is_symlink() and p.suffix.lower() in IMAGE_EXTENSIONS
    )
    labeled = 0
    negative_counter = [0]
    standard_layout = images_path.resolve() == (write_root / "images").resolve()

    for image_path in image_files:
        label_path = labels_dir / f"{image_path.stem}.txt"
        if label_path.exists() and label_path.stat().st_size > 0:
            continue

        frame = cv2.imread(str(image_path))
        if frame is None:
            continue

        if standard_layout:
            written = _process_frame(
                frame,
                segmenter,
                frame_index=0,
                output_dir=write_root,
                max_players=max_players,
                keep_negatives=keep_negatives,
                negative_every_n=negative_every_n,
                negative_counter=negative_counter,
                class_map=class_map,
                min_confidence=min_confidence,
                min_mask_area=min_mask_area,
                stem=image_path.stem,
                write_image=False,
            )
        else:
            written = _process_frame_to_label_path(
                frame,
                segmenter,
                label_path,
                max_players=max_players,
                keep_negatives=keep_negatives,
                negative_every_n=negative_every_n,
                negative_counter=negative_counter,
                class_map=class_map,
                min_confidence=min_confidence,
                min_mask_area=min_mask_area,
            )

        if written:
            labeled += 1
        if max_frames > 0 and labeled >= max_frames:
            break

    return labeled


def _process_frame_to_label_path(
    frame,
    segmenter: Cs2SamSegmenter,
    label_path: Path,
    *,
    max_players: int = 0,
    keep_negatives: bool = True,
    negative_every_n: int = 5,
    negative_counter: list[int] | None = None,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> int:
    """Predict and write a YOLO-seg label to an explicit path (non-standard layouts)."""
    if negative_counter is None:
        negative_counter = [0]

    results = segmenter.predict(frame, frame_index=0)
    h, w = frame.shape[:2]
    instances = list(results)

    if min_confidence is not None:
        instances = [inst for inst in instances if float(inst.confidence) >= float(min_confidence)]
    if min_mask_area > 0:
        instances = [
            inst for inst in instances if _polygon_area_px(tuple(inst.polygon)) >= min_mask_area
        ]

    instances.sort(key=lambda inst: inst.confidence, reverse=True)
    if max_players > 0 and len(instances) > max_players:
        instances = instances[:max_players]

    lines: list[str] = []
    for inst in instances:
        class_id = inst.class_id
        if class_map is not None:
            if class_id not in class_map:
                continue
            class_id = class_map[class_id]
        poly_norm = [(px / w, py / h) for px, py in inst.polygon]
        poly_str = " ".join(f"{px:.6f} {py:.6f}" for px, py in poly_norm)
        lines.append(f"{class_id} {poly_str}")

    if not lines:
        if not keep_negatives:
            return 0
        negative_counter[0] += 1
        every_n = negative_every_n if negative_every_n > 1 else 1
        if negative_counter[0] % every_n != 0:
            return 0

    label_path.parent.mkdir(parents=True, exist_ok=True)
    label_path.write_text(
        ("\n".join(lines) + "\n") if lines else "",
        encoding="utf-8",
    )
    return 1
