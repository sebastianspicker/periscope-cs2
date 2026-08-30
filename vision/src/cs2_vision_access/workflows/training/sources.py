from __future__ import annotations

import os
import re
import tempfile
from pathlib import Path
from typing import TypedDict

import cv2
import numpy as np

from cs2_vision_access.application.ports.segmentation import Segmenter
from cs2_vision_access.domain.predictions import InstanceMask
from cs2_vision_access.workflows.training.contracts import (
    Layout,
    classes_for_class_map,
)
from cs2_vision_access.workflows.training.contracts import (
    write_dataset_yaml as _contracts_write_dataset_yaml,
)

_FRAME_STEM_RE = re.compile(r"^frame_(\d+)$")


class _ProcessFrameKwargs(TypedDict):
    keep_negatives: bool
    negative_every_n: int
    negative_counter: list[int]
    class_map: dict[int, int] | None
    min_confidence: float | None
    min_mask_area: float


# ---------------------------------------------------------------------------
# Resume / layout helpers
# ---------------------------------------------------------------------------


def next_frame_index(output_dir: Path) -> int:
    """Return the next free ``frame_XXXXXXXX`` index under *output_dir*.

    Scans both ``images/`` and ``labels/`` for stems matching ``frame_<int>``
    and returns ``max_index + 1`` (or ``0`` when none exist).
    """
    max_idx = -1
    for sub in ("images", "labels"):
        directory = output_dir / sub
        if not directory.is_dir():
            continue
        for path in directory.iterdir():
            if not path.is_file():
                continue
            match = _FRAME_STEM_RE.match(path.stem)
            if match is None:
                continue
            max_idx = max(max_idx, int(match.group(1)))
    return max_idx + 1


def resolve_write_root(output_dir: Path, session_id: str | None = None) -> Path:
    """Resolve the directory that holds ``images/`` and ``labels/``.

    Default is flat ``output_dir/{images,labels}``. When *session_id* is set,
    write under ``output_dir/<session_id>/{images,labels}`` for staging
    assemble compatibility.
    """
    if session_id:
        return output_dir / session_id
    return output_dir


def ensure_image_label_dirs(write_root: Path) -> None:
    """Create ``images/`` and ``labels/`` under *write_root*."""
    (write_root / "images").mkdir(parents=True, exist_ok=True)
    (write_root / "labels").mkdir(parents=True, exist_ok=True)


def _polygon_area_px(polygon: tuple[tuple[float, float], ...]) -> float:
    """Shoelace polygon area in pixel units."""
    n = len(polygon)
    if n < 3:
        return 0.0
    area = 0.0
    for i in range(n):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % n]
        area += x1 * y2 - x2 * y1
    return abs(area) * 0.5


# ---------------------------------------------------------------------------
# Frame processing
# ---------------------------------------------------------------------------


def _process_frame(
    frame: np.ndarray,
    segmenter: Segmenter,
    frame_index: int,
    output_dir: Path,
    max_players: int,
    *,
    keep_negatives: bool = False,
    negative_every_n: int = 0,
    negative_counter: list[int] | None = None,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
    stem: str | None = None,
    write_image: bool = True,
) -> int:
    """Run cs2-sam on one frame, save image + label when warranted.

    Returns 1 if a label file (positive or empty negative) was written, else 0.

    Empty negatives: when no instances remain after filtering and
    ``keep_negatives`` is True, write an empty ``.txt`` + image either every
    ``negative_every_n`` empty attempts (``n > 1``) or on every empty attempt
    when ``negative_every_n <= 1`` (including the default ``0``).

    When *stem* is set, label/image basenames use that stem instead of
    ``frame_{frame_index:08d}``. When *write_image* is False, only the label
    file is written (existing frames in images-dir mode).
    """
    results = segmenter.predict(frame, frame_index=0)
    h, w = frame.shape[:2]

    instances: list[InstanceMask] = list(results)

    # Confidence floor (post-predict; segmenter may already threshold).
    if min_confidence is not None:
        instances = [inst for inst in instances if inst.confidence >= min_confidence]

    # Drop tiny masks (pixel area of polygon).
    if min_mask_area > 0.0:
        instances = [inst for inst in instances if _polygon_area_px(inst.polygon) >= min_mask_area]

    # Sort by confidence descending before max_players cap.
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
        # Track empty attempts for every-N sampling.
        if negative_counter is not None:
            negative_counter[0] += 1
            empty_count = negative_counter[0]
        else:
            empty_count = 1
        every_n = negative_every_n if negative_every_n > 1 else 1
        if empty_count % every_n != 0:
            return 0
        # Fall through with empty lines → write empty negative.

    img_name = stem if stem is not None else f"frame_{frame_index:08d}"
    label_path = output_dir / "labels" / f"{img_name}.txt"

    if write_image:
        img_path = output_dir / "images" / f"{img_name}.jpg"
        cv2.imwrite(str(img_path), frame, [cv2.IMWRITE_JPEG_QUALITY, 90])
    label_path.parent.mkdir(parents=True, exist_ok=True)
    label_path.write_text(("\n".join(lines) + "\n") if lines else "", encoding="utf-8")
    return 1


# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------


def _default_process_kwargs(
    *,
    keep_negatives: bool = False,
    negative_every_n: int = 0,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> _ProcessFrameKwargs:
    return {
        "keep_negatives": keep_negatives,
        "negative_every_n": negative_every_n,
        "negative_counter": [0],
        "class_map": class_map,
        "min_confidence": min_confidence,
        "min_mask_area": min_mask_area,
    }


def _from_video(
    video_path: str,
    segmenter: Segmenter,
    output_dir: Path,
    sample_rate: float,
    max_frames: int,
    max_players: int,
    *,
    start_index: int | None = None,
    keep_negatives: bool = False,
    negative_every_n: int = 0,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> int:
    """Process a video file frame by frame.

    Frame indices start at ``next_frame_index(output_dir)`` when *start_index*
    is omitted so re-runs append rather than overwrite.
    """
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError(f"cannot open video: {video_path}")

    fps = cap.get(cv2.CAP_PROP_FPS)
    sample_interval = max(1, int(round(fps / sample_rate))) if fps > 0 else 16
    print(f"  Video FPS: {fps:.1f}, sampling every {sample_interval} frames")

    frame_idx = 0
    labeled = 0
    write_index = next_frame_index(output_dir) if start_index is None else start_index
    proc_kw = _default_process_kwargs(
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        class_map=class_map,
        min_confidence=min_confidence,
        min_mask_area=min_mask_area,
    )

    while True:
        ret, frame = cap.read()
        if not ret:
            break
        if frame_idx % sample_interval == 0:
            if _process_frame(frame, segmenter, write_index, output_dir, max_players, **proc_kw):
                labeled += 1
                write_index += 1
            if labeled % 50 == 0 and labeled > 0:
                print(f"  {labeled} frames labeled...")
        frame_idx += 1
        if max_frames > 0 and labeled >= max_frames:
            break

    cap.release()
    return labeled


def _from_tar(
    tar_path: str,
    segmenter: Segmenter,
    output_dir: Path,
    sample_rate: float,
    max_frames: int,
    max_players: int,
    *,
    start_index: int | None = None,
    keep_negatives: bool = False,
    negative_every_n: int = 0,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> int:
    """Process a WebDataset tar shard (CS2-10k format)."""
    try:
        import webdataset as wds
    except ImportError:
        raise RuntimeError(
            "webdataset is required for --tar mode; pip install webdataset"
        ) from None

    dataset = wds.WebDataset(tar_path).decode()
    labeled = 0
    write_index = next_frame_index(output_dir) if start_index is None else start_index
    proc_kw = _default_process_kwargs(
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        class_map=class_map,
        min_confidence=min_confidence,
        min_mask_area=min_mask_area,
    )

    for sample in dataset:
        video_data = sample.get("mp4")
        if video_data is None:
            continue

        with tempfile.NamedTemporaryFile(suffix=".mp4", delete=False) as f:
            f.write(video_data)
            tmp_path = f.name

        cap = cv2.VideoCapture(tmp_path)
        fps = cap.get(cv2.CAP_PROP_FPS)
        sample_interval = max(1, int(round(fps / sample_rate))) if fps > 0 else 48

        vid_frame_idx = 0
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            if vid_frame_idx % sample_interval == 0:
                if _process_frame(
                    frame, segmenter, write_index, output_dir, max_players, **proc_kw
                ):
                    labeled += 1
                    write_index += 1
                if labeled % 50 == 0 and labeled > 0:
                    print(f"  {labeled} frames labeled...")
                if max_frames > 0 and labeled >= max_frames:
                    break
            vid_frame_idx += 1

        cap.release()
        os.unlink(tmp_path)

        if max_frames > 0 and labeled >= max_frames:
            break

    return labeled


def _from_live_screen(
    segmenter: Segmenter,
    output_dir: Path,
    max_frames: int,
    max_players: int,
    *,
    start_index: int | None = None,
    keep_negatives: bool = False,
    negative_every_n: int = 0,
    class_map: dict[int, int] | None = None,
    min_confidence: float | None = None,
    min_mask_area: float = 0.0,
) -> int:
    """Live CS2 screen capture; press 'S' to save a frame, 'Q' to quit."""
    try:
        import mss
    except ImportError:
        raise RuntimeError(
            "mss is required for --live-screen; install project dependencies"
        ) from None

    monitor = mss.mss().monitors[1]  # primary monitor
    labeled = 0
    write_index = next_frame_index(output_dir) if start_index is None else start_index
    proc_kw = _default_process_kwargs(
        keep_negatives=keep_negatives,
        negative_every_n=negative_every_n,
        class_map=class_map,
        min_confidence=min_confidence,
        min_mask_area=min_mask_area,
    )

    print("Live capture started. Press 'S' to save current frame, 'Q' to quit.")
    cv2.namedWindow("CS2 Self-Train (S=save, Q=quit)", cv2.WINDOW_NORMAL)

    with mss.mss() as sct:
        while True:
            bgr_screenshot = np.array(sct.grab(monitor))[:, :, :3]
            bgr = bgr_screenshot[:, :, :3]

            cv2.imshow("CS2 Self-Train (S=save, Q=quit)", bgr)
            key = cv2.waitKey(1) & 0xFF

            if key == ord("q") or key == 27:
                break
            if key == ord("s") or key == ord(" "):
                if _process_frame(bgr, segmenter, write_index, output_dir, max_players, **proc_kw):
                    labeled += 1
                    write_index += 1
                    print(f"  Saved frame {labeled}")
                else:
                    print("  No players detected in this frame — not saved")
                if max_frames > 0 and labeled >= max_frames:
                    print(f"Reached max frames ({max_frames})")
                    break

    cv2.destroyAllWindows()
    return labeled


# ---------------------------------------------------------------------------
# Dataset config writer
# ---------------------------------------------------------------------------


def _write_dataset_yaml(
    output_dir: Path,
    *,
    classes: dict[int, str] | None = None,
    class_map: dict[int, int] | None = None,
) -> None:
    """Write Ultralytics dataset.yaml via :mod:`training.contracts`.

    If *classes* is omitted, uses PRODUCT when *class_map* collapses all sources
    onto a single destination id ``0``, otherwise VOMBIT.
    """
    if classes is None:
        classes = classes_for_class_map(class_map)
    path = _contracts_write_dataset_yaml(
        output_dir,
        layout=Layout.FLAT_BOOTSTRAP,
        classes=classes,
        portable_path=False,
    )
    print(f"  Dataset config: {path} ({list(classes.values())})")
