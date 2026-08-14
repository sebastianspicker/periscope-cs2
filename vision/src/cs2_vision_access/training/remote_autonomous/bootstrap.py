"""Sparse-label bootstrap via EdgeSAM and/or COCO person shape prior."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path

from . import deps
from .fsutil import _assert_labels_compatible


def _resolve_edgesam_artifacts_dir(
    data_dir: Path,
    edgesam_artifacts_dir: str | Path | None,
) -> Path:
    """Resolve EdgeSAM asset directory (explicit, existing artifacts/, or data_dir)."""
    if edgesam_artifacts_dir is not None:
        return Path(edgesam_artifacts_dir)
    from cs2_vision_access.training.prepare_lib import discover_edgesam_assets

    for candidate in (Path("artifacts"), data_dir / "edgesam_assets"):
        if not candidate.is_dir():
            continue
        try:
            discover_edgesam_assets(candidate)
            return candidate
        except FileNotFoundError:
            continue
    return data_dir / "edgesam_assets"


def _phase_bootstrap_labels(
    data_dir: Path,
    *,
    images_dir: Path,
    labels_dir: Path,
    n_images: int,
    n_labels: int,
    bootstrap_if_needed: bool,
    min_label_ratio: float,
    use_edgesam: bool,
    download_edgesam: bool,
    edgesam_artifacts_dir: str | Path | None,
    edgesam_confidence: float,
    edgesam_coco_fallback: bool,
    bootstrap_conf: float,
    device: str,
    class_map: Mapping[int, str],
    notes: list[str],
) -> tuple[int, int]:
    """Sparse-label bootstrap via EdgeSAM and/or COCO person shape prior.

    Returns ``(bootstrap_written, n_labels)``.
    """
    bootstrap_written = 0
    label_ratio = n_labels / max(1, n_images)
    if bootstrap_if_needed and label_ratio < min_label_ratio:
        notes.append(
            f"label coverage {label_ratio:.1%} < {min_label_ratio:.0%}; bootstrapping silhouettes"
        )
        used_edgesam = False
        if use_edgesam:
            artifacts_dir = _resolve_edgesam_artifacts_dir(data_dir, edgesam_artifacts_dir)
            notes.append(f"edgesam artifacts_dir={artifacts_dir}")
            print(
                "Sparse labels — EdgeSAM auto-label "
                f"(artifacts={artifacts_dir}, download={download_edgesam})..."
            )
            edgesam_result = deps.bootstrap_with_edgesam(
                data_dir,
                images_dirs=None,  # images/ + images_val via candidate_image_dirs
                artifacts_dir=artifacts_dir,
                download=bool(download_edgesam),
                device=device,
                confidence=float(edgesam_confidence),
                collapse_to_player=True,
                keep_negatives=True,
                class_names=dict(class_map),
            )
            notes.extend(edgesam_result.notes)
            bootstrap_written = int(edgesam_result.labeled_frames)
            n_labels = deps.count_labels(labels_dir)
            used_edgesam = bool(edgesam_result.ok) and n_labels > 0
            if edgesam_result.ok:
                notes.append(f"edgesam prepare labeled={bootstrap_written} labels_now={n_labels}")
                print(
                    f"EdgeSAM auto-label complete: labeled={bootstrap_written}, "
                    f"labels={n_labels}/{n_images}"
                )
            else:
                print(f"EdgeSAM failed or incomplete; notes={edgesam_result.notes[-1:]}")
                if not edgesam_coco_fallback:
                    raise RuntimeError(
                        edgesam_result.notes[-1]
                        if edgesam_result.notes
                        else "edgesam bootstrap failed"
                    )
                used_edgesam = False
            if edgesam_result.ok and not used_edgesam:
                notes.append(
                    "edgesam produced no labels; "
                    + (
                        "falling back to COCO person"
                        if edgesam_coco_fallback
                        else "no COCO fallback"
                    )
                )
                print(
                    "EdgeSAM wrote no labels; "
                    + (
                        "falling back to COCO person bootstrap..."
                        if edgesam_coco_fallback
                        else "COCO fallback disabled."
                    )
                )

        if not used_edgesam and (not use_edgesam or edgesam_coco_fallback):
            print(
                "Sparse labels — bootstrapping player-shaped polygons with YOLO-seg "
                "COCO person (shape prior for CS2 silhouettes)..."
            )
            # Product player is class 0 in PRODUCT_CLASSES; remapped from COCO person.
            cid = deps.bootstrap_class_id(class_map)
            bootstrap_written = deps.bootstrap_labels_with_yolo_person(
                data_dir,
                device=device,
                conf=bootstrap_conf,
                class_id=cid,
                classes=class_map,
            )
            n_labels = deps.count_labels(labels_dir)
            notes.append(f"coco person bootstrap labeled={bootstrap_written} class_id={cid}")
            print(
                f"After bootstrap: labels={n_labels}/{n_images} "
                f"(+{bootstrap_written} newly written). "
                "Shapes are pseudo-labeled player silhouettes "
                f"(COCO person → class_id={cid})."
            )
    else:
        deps.write_dataset_yaml(
            data_dir,
            layout=deps.Layout.FLAT_BOOTSTRAP,
            classes=class_map,
            portable_path=False,
        )

    if n_labels == 0:
        raise RuntimeError(
            "No labels after bootstrap. Check images and bootstrap conf, or "
            "upload a zip that already includes labels/."
        )

    # Class-id SSOT check on flat labels before split / train.
    for w in _assert_labels_compatible([labels_dir], class_map):
        notes.append(f"label check: {w}")

    return bootstrap_written, n_labels
