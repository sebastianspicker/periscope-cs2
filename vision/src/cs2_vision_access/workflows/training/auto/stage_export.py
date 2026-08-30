"""Export stage for auto-train pipelines."""

from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.workflows.training.cloud import package_outputs

from .config import AutoTrainConfig
from .errors import AutoTrainStageError
from .notes import append_soft_note
from .paths import RunPaths
from .state import StageState


def _find_smoke_frame(
    config: AutoTrainConfig,
    state: StageState,
) -> Path | None:
    """Locate a single image or video frame for smoke inference."""
    if config.sources.videos:
        video = Path(config.sources.videos[0])
        if video.is_file():
            return video
    dataset_root = state.artifacts.get("dataset_root")
    if not dataset_root:
        return None
    root = Path(str(dataset_root))
    images_dir = root / "images"
    search_roots = [images_dir]
    for split in ("val", "train"):
        candidate = images_dir / split
        if candidate.is_dir():
            search_roots.insert(0, candidate)
    for search in search_roots:
        if not search.is_dir():
            continue
        for path in sorted(search.rglob("*")):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                return path
    return None


def _load_smoke_frame(source: Path) -> Any | None:
    """Load a BGR ndarray from an image path or the first frame of a video."""
    try:
        import cv2
    except ImportError:
        return None
    suffix = source.suffix.lower()
    if suffix in IMAGE_EXTENSIONS:
        frame = cv2.imread(str(source), cv2.IMREAD_COLOR)
        return frame
    # Video path
    capture = cv2.VideoCapture(str(source))
    try:
        ok, frame = capture.read()
        if not ok:
            return None
        return frame
    finally:
        capture.release()


def stage_export(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
) -> dict[str, Any]:
    """Promote ONNX + manifest into run models/ and optionally package a zip."""
    onnx_raw = state.artifacts.get("onnx_model")
    manifest_raw = state.artifacts.get("manifest")
    if not onnx_raw or not manifest_raw:
        raise AutoTrainStageError("export requires onnx_model and manifest from train")
    onnx_src = Path(str(onnx_raw))
    manifest_src = Path(str(manifest_raw))
    if not onnx_src.is_file():
        raise AutoTrainStageError(f"onnx_model not found: {onnx_src}")
    if not manifest_src.is_file():
        raise AutoTrainStageError(f"manifest not found: {manifest_src}")

    updates: dict[str, Any] = {}
    if config.export.promote_to_run_models:
        paths.models_dir.mkdir(parents=True, exist_ok=True)
        dest_onnx = paths.models_dir / onnx_src.name
        dest_manifest = paths.models_dir / manifest_src.name
        shutil.copy2(onnx_src, dest_onnx)
        shutil.copy2(manifest_src, dest_manifest)
        updates["promoted_onnx"] = str(dest_onnx.resolve())
        updates["promoted_manifest"] = str(dest_manifest.resolve())
        updates["onnx_model"] = updates["promoted_onnx"]
        updates["manifest"] = updates["promoted_manifest"]
    else:
        updates["onnx_model"] = str(onnx_src.resolve())
        updates["manifest"] = str(manifest_src.resolve())

    if config.mode == "flat_cloud":
        package_zip = paths.package_dir / config.export.package_zip_name
        package_outputs(
            updates["onnx_model"],
            updates["manifest"],
            package_zip,
        )
        updates["package_zip"] = str(package_zip.resolve())

    # Smoke inference: try ONNX segmenter on one frame/video when available.
    if not config.export.smoke_enabled:
        updates["smoke_inference"] = "disabled"
        updates["smoke_ok"] = None
        return updates

    smoke_source = _find_smoke_frame(config, state)
    if smoke_source is None:
        updates["smoke_inference"] = "skipped_no_source"
        updates["smoke_ok"] = None
    else:
        try:
            from cs2_vision_access.application.ports.segmentation import create_segmenter
        except ImportError as error:
            updates["smoke_inference"] = "import_failed"
            updates["smoke_ok"] = False
            append_soft_note(updates, f"smoke inference import failed: {error}")
        else:
            try:
                segmenter = create_segmenter(
                    Path(updates["onnx_model"]),
                    Path(updates["manifest"]),
                    confidence=0.25,
                    device=config.train.device,
                )
                frame = _load_smoke_frame(smoke_source)
                if frame is None:
                    updates["smoke_inference"] = "skipped_no_frame"
                    updates["smoke_ok"] = None
                    append_soft_note(updates, f"smoke could not load frame from {smoke_source}")
                else:
                    _ = list(segmenter.predict(frame, frame_index=0))
                    updates["smoke_inference"] = "ok"
                    updates["smoke_ok"] = True
                    updates["smoke_source"] = str(smoke_source)
            except Exception as error:  # noqa: BLE001 — soft fail unless required
                updates["smoke_inference"] = "failed"
                updates["smoke_ok"] = False
                append_soft_note(updates, f"smoke inference failed: {error}")

    if config.export.smoke_required and updates.get("smoke_ok") is not True:
        raise AutoTrainStageError(
            f"export smoke inference required but not ok "
            f"(smoke_inference={updates.get('smoke_inference')!r})"
        )

    return updates
