"""EdgeSAM bootstrap orchestration over image directories."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path

from cs2_vision_access.dataset.types import IMAGE_EXTENSIONS
from cs2_vision_access.training.prepare_lib.assets import ensure_edgesam_assets
from cs2_vision_access.training.prepare_lib.prepare_run import run_cs2_sam_prepare


def candidate_image_dirs_for_labeling(
    data_dir: Path | str,
    *,
    images_dir: Path | str | None = None,
    include_images_val: bool = True,
) -> list[Path]:
    """Return existing dirs with images: primary images/ + optional images_val/.

    *images_dir* defaults to ``data_dir/images``. Directories are included only
    when they exist and contain at least one image file. Duplicates (same
    resolved path) are skipped.
    """
    root = Path(data_dir)
    primary = Path(images_dir) if images_dir is not None else root / "images"
    out: list[Path] = []
    seen: set[Path] = set()

    def _has_images(directory: Path) -> bool:
        if not directory.is_dir():
            return False
        try:
            for path in directory.iterdir():
                if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS:
                    return True
        except OSError:
            return False
        return False

    def _add(directory: Path) -> None:
        if not _has_images(directory):
            return
        try:
            key = directory.resolve()
        except OSError:
            key = directory
        if key in seen:
            return
        seen.add(key)
        out.append(directory)

    _add(primary)
    if include_images_val:
        _add(root / "images_val")
    return out


@dataclass
class EdgesamBootstrapResult:
    """Outcome of :func:`bootstrap_with_edgesam`."""

    labeled_frames: int
    assets: dict[str, Path] | None
    notes: list[str] = field(default_factory=list)
    ok: bool = False


def bootstrap_with_edgesam(
    data_dir: Path | str,
    *,
    images_dirs: list[Path] | None = None,
    artifacts_dir: Path | str | None = None,
    download: bool = False,
    device: str = "cpu",
    confidence: float = 0.4,
    collapse_to_player: bool = True,
    keep_negatives: bool = True,
    explicit_assets: dict[str, Path] | None = None,
    max_frames: int = 0,
    class_names: Mapping[int, str] | None = None,
    write_yaml: bool = True,
) -> EdgesamBootstrapResult:
    """Resolve EdgeSAM assets and label image directories under *data_dir*.

    Soft-failure API: returns ``ok=False`` with notes instead of raising when
    assets are missing or prepare fails, so callers can fall back (e.g. COCO).

    1. Resolve assets: *explicit_assets*, else
       :func:`ensure_edgesam_assets`(*artifacts_dir*, download=*download*)
       (discover-only when *download* is False).
    2. For each images dir (or :func:`candidate_image_dirs_for_labeling` when
       *images_dirs* is None), run :func:`run_cs2_sam_prepare`.
    3. Return total labeled frames + notes; ``ok=True`` when assets resolved and
       prepare completed without error (even if zero frames were labeled).
    """
    data_path = Path(data_dir)
    notes: list[str] = []

    # --- assets ------------------------------------------------------------
    assets: dict[str, Path] | None = None
    try:
        if explicit_assets is not None:
            required = ("detector", "manifest", "encoder", "decoder")
            missing = [k for k in required if k not in explicit_assets]
            if missing:
                raise FileNotFoundError(f"explicit_assets missing keys: {', '.join(missing)}")
            assets = {k: Path(explicit_assets[k]) for k in required}
            for key, path in assets.items():
                if not path.is_file():
                    raise FileNotFoundError(f"{key} not found: {path}")
        else:
            art = Path(artifacts_dir) if artifacts_dir is not None else data_path / "edgesam_assets"
            assets = ensure_edgesam_assets(art, download=bool(download))
        notes.append("edgesam assets: " + ", ".join(f"{k}={v.name}" for k, v in assets.items()))
    except Exception as exc:  # noqa: BLE001 — soft for callers
        notes.append(f"edgesam assets failed: {exc}")
        return EdgesamBootstrapResult(
            labeled_frames=0,
            assets=None,
            notes=notes,
            ok=False,
        )

    # --- image dirs --------------------------------------------------------
    if images_dirs is None:
        dirs = candidate_image_dirs_for_labeling(data_path)
    else:
        dirs = [Path(d) for d in images_dirs]
        dirs = [d for d in dirs if d.is_dir()]

    if not dirs:
        notes.append(f"no images to EdgeSAM-label under {data_path} (images/ or images_val/)")
        return EdgesamBootstrapResult(
            labeled_frames=0,
            assets=assets,
            notes=notes,
            ok=False,
        )

    total = 0
    try:
        for i, img_dir in enumerate(dirs):
            result = run_cs2_sam_prepare(
                detector=assets["detector"],
                manifest=assets["manifest"],
                encoder=assets["encoder"],
                decoder=assets["decoder"],
                output_dir=data_path,
                images_dir=img_dir,
                device=device,
                confidence=confidence,
                collapse_to_player=collapse_to_player,
                keep_negatives=keep_negatives,
                max_frames=max_frames,
                write_yaml=write_yaml and (i == len(dirs) - 1),
                class_names=class_names,
            )
            total += int(result.labeled_frames)
            notes.append(
                f"edgesam labeled {result.labeled_frames} under {img_dir} "
                f"(write_root={result.write_root})"
            )
            print(
                f"  EdgeSAM labeled {result.labeled_frames} under {img_dir} "
                f"(write_root={result.write_root})"
            )
    except Exception as exc:  # noqa: BLE001 — soft for callers
        notes.append(f"edgesam prepare failed: {exc}")
        return EdgesamBootstrapResult(
            labeled_frames=total,
            assets=assets,
            notes=notes,
            ok=False,
        )

    notes.append(f"edgesam prepare labeled={total}")
    return EdgesamBootstrapResult(
        labeled_frames=total,
        assets=assets,
        notes=notes,
        ok=True,
    )
