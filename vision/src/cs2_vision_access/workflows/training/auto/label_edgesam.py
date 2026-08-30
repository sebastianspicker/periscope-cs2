"""EdgeSAM teacher helpers for the auto-train label stage."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from cs2_vision_access.workflows.training.dataset_zip import count_images, count_labels

from .config import AutoTrainConfig
from .dataset_helpers import (
    count_labels_recursive,
    label_coverage_ok,
    labels_sufficient,
    write_session_dataset_yaml,
)
from .errors import AutoTrainStageError
from .notes import append_soft_note
from .paths import RunPaths
from .state import StageState


def _resolve_label_assets(label: Any) -> dict[str, Path]:
    """Resolve explicit label.* paths or discover/ensure under artifacts_dir.

    When ``label.download_edgesam`` is True, missing packs may be downloaded via
    :func:`ensure_edgesam_assets`; otherwise offline discover only.

    Returns dict with keys detector, manifest, encoder, decoder.
    """
    from cs2_vision_access.workflows.training.prepare_lib import (
        discover_edgesam_assets,
        ensure_edgesam_assets,
    )

    explicit = {
        "detector": label.detector,
        "manifest": label.detector_manifest,
        "encoder": label.encoder,
        "decoder": label.decoder,
    }
    if all(v is not None for v in explicit.values()):
        out: dict[str, Path] = {}
        for key, value in explicit.items():
            path = Path(value)
            if not path.is_file():
                raise FileNotFoundError(
                    f"label.{key if key != 'manifest' else 'detector_manifest'} not a file: {path}"
                )
            out[key] = path.resolve()
        return out

    download = bool(getattr(label, "download_edgesam", False))
    if download:
        discovered = ensure_edgesam_assets(label.artifacts_dir, download=True)
    else:
        discovered = discover_edgesam_assets(label.artifacts_dir)
    # Fill missing fields from discovery; explicit wins.
    result: dict[str, Path] = {}
    for key, value in explicit.items():
        if value is not None:
            path = Path(value)
            if not path.is_file():
                field = "detector_manifest" if key == "manifest" else key
                raise FileNotFoundError(f"label.{field} not a file: {path}")
            result[key] = path.resolve()
        else:
            result[key] = Path(discovered[key]).resolve()
    return result


def _run_edgesam_teacher(
    config: AutoTrainConfig,
    paths: RunPaths,
    state: StageState,
    *,
    root: Path,
    is_session: bool,
    is_flat: bool,
    n_images: int,
    n_labels: int,
    min_labels: int,
    min_label_ratio: float,
    notes: list[str],
    updates: dict[str, Any],
) -> dict[str, Any] | None:
    """Run EdgeSAM prepare. Return updates dict, or None to fall through to coco.

    Priority:
    1. Label assembled dataset images in place (images/train or flat images/)
    2. Else label listed videos into staging
    3. Else skip with note
    """
    label = config.label
    try:
        from cs2_vision_access.workflows.training.prepare_lib import run_cs2_sam_prepare
    except ImportError as error:
        notes.append(f"edgesam import failed (soft): {error}")
        append_soft_note(updates, notes[-1])
        if label.coco_fallback and label.bootstrap:
            notes.append("edgesam soft-fail → coco_person fallback")
            return None
        updates["label_status"] = "skipped_import"
        updates["label_notes"] = notes
        updates["edgesam_notes"] = list(notes)
        if label.required:
            raise AutoTrainStageError(
                f"label.required=true but edgesam import failed: {error}"
            ) from error
        return updates

    try:
        assets = _resolve_label_assets(label)
        updates["edgesam_assets"] = {k: str(v) for k, v in assets.items()}
    except FileNotFoundError as error:
        notes.append(f"edgesam assets missing (soft): {error}")
        append_soft_note(updates, notes[-1])
        updates["edgesam_notes"] = list(notes)
        if label.required:
            updates["label_status"] = "edgesam_assets_missing"
            updates["label_notes"] = notes
            raise AutoTrainStageError(
                f"label.required=true but edgesam assets missing: {error}"
            ) from error
        if label.coco_fallback and label.bootstrap:
            notes.append("edgesam assets missing → coco_person fallback")
            return None
        updates["label_status"] = "skipped_edgesam_assets"
        updates["label_notes"] = notes
        return updates

    total_labeled = 0
    try:
        if is_session and n_images > 0:
            splits = tuple(label.bootstrap_splits) or ("train",)
            updates["bootstrap_splits"] = list(splits)
            for split in splits:
                split_images = root / "images" / split
                split_labels = root / "labels" / split
                split_n_images = count_images(split_images) if split_images.is_dir() else 0
                split_n_labels = count_labels(split_labels) if split_labels.is_dir() else 0
                if split_n_images == 0:
                    notes.append(f"edgesam skip split={split}: no images")
                    continue
                if label_coverage_ok(
                    split_n_labels,
                    split_n_images,
                    min_labels=min_labels,
                    min_label_ratio=min_label_ratio,
                ):
                    notes.append(
                        f"edgesam skip split={split}: already sufficient "
                        f"({split_n_labels}/{split_n_images})"
                    )
                    continue
                result = run_cs2_sam_prepare(
                    detector=assets["detector"],
                    manifest=assets["manifest"],
                    encoder=assets["encoder"],
                    decoder=assets["decoder"],
                    output_dir=root,
                    images_dir=split_images,
                    labels_dir=split_labels,
                    device=config.train.device,
                    sample_rate=label.sample_rate,
                    confidence=label.edgesam_confidence,
                    max_frames=label.max_frames,
                    collapse_to_player=label.collapse_to_player,
                    keep_negatives=label.keep_negatives,
                    write_yaml=False,
                    class_names=config.train.class_names,
                )
                written = int(result.labeled_frames)
                total_labeled += written
                notes.append(
                    f"edgesam split={split}: labeled {written} "
                    f"({split_n_labels}/{split_n_images} before)"
                )
            yaml_path = write_session_dataset_yaml(root, config.train.class_names)
            updates["dataset_yaml"] = str(yaml_path.resolve())
            updates["edgesam_labeled"] = total_labeled
        elif is_flat and n_images > 0:
            from cs2_vision_access.workflows.training.prepare_lib import (
                bootstrap_with_edgesam,
                candidate_image_dirs_for_labeling,
            )

            image_dirs = candidate_image_dirs_for_labeling(root)
            if not image_dirs:
                image_dirs = [root / "images"]
            boot = bootstrap_with_edgesam(
                root,
                images_dirs=image_dirs,
                explicit_assets=assets,
                device=config.train.device or "cpu",
                confidence=label.edgesam_confidence,
                collapse_to_player=label.collapse_to_player,
                keep_negatives=label.keep_negatives,
                max_frames=label.max_frames,
                class_names=config.train.class_names,
                write_yaml=True,
            )
            notes.extend(boot.notes)
            if not boot.ok:
                raise RuntimeError(
                    boot.notes[-1] if boot.notes else "edgesam flat bootstrap failed"
                )
            total_labeled = int(boot.labeled_frames)
            notes.append(f"edgesam flat: labeled {total_labeled}")
            updates["edgesam_labeled"] = total_labeled
        else:
            videos = _resolve_label_videos(config, state)
            if not videos:
                notes.append("edgesam: no sparse images under dataset_root and no videos; skip")
                updates["label_status"] = "skipped_no_edgesam_target"
                updates["edgesam_notes"] = list(notes)
                updates["label_notes"] = notes
                if label.coco_fallback and label.bootstrap:
                    notes.append("edgesam no target → coco_person fallback")
                    return None
                if label.required:
                    raise AutoTrainStageError(
                        "label.required=true but edgesam has no images or videos to label"
                    )
                return updates

            staging_root = Path(
                str(
                    state.artifacts.get("staging_root")
                    or config.sources.staging_root
                    or paths.staging_dir
                )
            )
            staging_root.mkdir(parents=True, exist_ok=True)
            for video in videos:
                if not video.is_file():
                    notes.append(f"edgesam skip missing video: {video}")
                    continue
                session_id = video.stem
                result = run_cs2_sam_prepare(
                    detector=assets["detector"],
                    manifest=assets["manifest"],
                    encoder=assets["encoder"],
                    decoder=assets["decoder"],
                    output_dir=staging_root,
                    video=video,
                    session_id=session_id,
                    device=config.train.device,
                    sample_rate=label.sample_rate,
                    confidence=label.edgesam_confidence,
                    max_frames=label.max_frames,
                    collapse_to_player=label.collapse_to_player,
                    keep_negatives=label.keep_negatives,
                    write_yaml=True,
                    class_names=config.train.class_names,
                )
                written = int(result.labeled_frames)
                total_labeled += written
                notes.append(f"edgesam video={video.name} session={session_id}: labeled {written}")
            updates["edgesam_labeled"] = total_labeled
            updates["staging_root"] = str(staging_root.resolve())

            plan_path = state.artifacts.get("split_plan") or (
                str(config.sources.split_plan) if config.sources.split_plan is not None else None
            )
            if plan_path and config.mode == "session_split":
                try:
                    import shutil

                    from cs2_vision_access.workflows.dataset.split import (
                        assemble_dataset,
                        load_split_plan,
                    )

                    plan = load_split_plan(plan_path)
                    if paths.dataset_dir.exists():
                        for child in (
                            "images",
                            "labels",
                            "sessions.json",
                            "dataset.yaml",
                        ):
                            target = paths.dataset_dir / child
                            if target.is_dir():
                                shutil.rmtree(target)
                            elif target.is_file():
                                target.unlink()
                    summary = assemble_dataset(
                        staging_root,
                        paths.dataset_dir,
                        plan,
                        overwrite=True,
                    )
                    yaml_path = write_session_dataset_yaml(
                        paths.dataset_dir, config.train.class_names
                    )
                    updates["dataset_root"] = str(paths.dataset_dir.resolve())
                    updates["dataset_yaml"] = str(yaml_path.resolve())
                    updates["assemble_summary"] = summary.as_dict()
                    notes.append("edgesam: re-assembled dataset after video labeling")
                    root = paths.dataset_dir
                except Exception as assemble_error:  # noqa: BLE001
                    notes.append(f"edgesam re-assemble soft-fail: {assemble_error}")
                    append_soft_note(updates, notes[-1])

        n_after = count_labels_recursive(root / "labels") if (root / "labels").is_dir() else 0
        updates["label_status"] = "edgesam"
        updates["labels_written"] = total_labeled
        updates["label_count"] = n_after if n_after else total_labeled
        notes.append(f"edgesam total labeled={total_labeled}; label files under dataset≈{n_after}")
        updates["edgesam_notes"] = list(notes)
    except Exception as error:  # noqa: BLE001 — soft fail unless required
        notes.append(f"edgesam failed (soft): {error}")
        append_soft_note(updates, notes[-1])
        updates["edgesam_notes"] = list(notes)
        if label.required:
            updates["label_status"] = "edgesam_failed"
            updates["label_notes"] = notes
            raise AutoTrainStageError(f"label.required=true but edgesam failed: {error}") from error
        if label.coco_fallback and label.bootstrap:
            notes.append("edgesam failed → coco_person fallback")
            return None
        updates["label_status"] = "edgesam_failed"
        updates["label_notes"] = notes
        return updates

    still_sparse = not labels_sufficient(
        root,
        min_labels=min_labels,
        min_label_ratio=min_label_ratio,
        mode=config.mode,
    )
    if still_sparse and (is_session or is_flat):
        n_after = int(updates.get("label_count") or 0)
        sparse_msg = (
            f"labels remain sparse after edgesam "
            f"({n_after} labels / {n_images} images; "
            f"min_labels={min_labels}, min_label_ratio={min_label_ratio})"
        )
        notes.append(sparse_msg)
        updates["label_status"] = "edgesam_insufficient"
        append_soft_note(updates, sparse_msg)
        if label.required:
            updates["label_notes"] = notes
            updates["edgesam_notes"] = list(notes)
            raise AutoTrainStageError("label.required=true but " + sparse_msg)
        if label.coco_fallback and label.bootstrap:
            notes.append("edgesam insufficient → coco_person fallback")
            updates["edgesam_notes"] = list(notes)
            return None

    updates["label_notes"] = notes
    updates["edgesam_notes"] = list(notes)
    return updates


def _resolve_label_videos(config: AutoTrainConfig, state: StageState) -> list[Path]:
    raw = state.artifacts.get("videos") or [str(v) for v in config.sources.videos]
    out: list[Path] = []
    for item in raw:
        path = Path(str(item))
        if path.is_file():
            out.append(path)
    return out
