"""Dataset resolution: zip discovery/extract and CS2-10k materialization."""

from __future__ import annotations

from pathlib import Path

from . import deps
from .fsutil import _ensure_dirs
from .models import (
    _CS2_10K_LICENSE,
    _CS2_10K_SOURCE,
    _ResolvedData,
)


def _phase_resolve_data(
    data_dir: Path,
    *,
    dataset_zip: str | Path | None,
    search_roots: list[str | Path] | None,
    use_cs2_10k: bool,
    cs2_10k_maps: tuple[str, ...],
    cs2_10k_max_shards: int,
    cs2_10k_max_videos: int,
    cs2_10k_frames_per_video: int,
    cs2_10k_cache_dir: str | Path | None,
    cs2_10k_holdout_video_fraction: float,
    notes: list[str],
) -> _ResolvedData:
    """Zip / CS2-10k / existing images — materialize a local data_dir with frames."""
    zip_path: Path | None = Path(dataset_zip) if dataset_zip else None
    if zip_path is not None and not zip_path.is_file():
        zip_path = None
    if zip_path is None and not (data_dir / "images").is_dir():
        found = deps.find_dataset_zip(search_roots)
        if found is not None:
            zip_path = found
            notes.append(f"auto-discovered zip: {found}")

    if zip_path is not None:
        print(f"Extracting {zip_path} → {data_dir}")
        data_dir = deps.extract_dataset(zip_path, data_dir)
    else:
        data_dir.mkdir(parents=True, exist_ok=True)
        notes.append("using existing data_dir (no zip)")

    images_dir, labels_dir = _ensure_dirs(data_dir)
    n_images = deps.count_images(images_dir)
    n_labels = deps.count_labels(labels_dir)

    dataset_license: str | None = None
    dataset_source: str | None = None

    # --- Auto-load CS2-10k when no local frames -----------------------------
    if n_images == 0 and use_cs2_10k:
        print(f"No local images — downloading a small slice of RekaAI/CS2-10k ({_CS2_10K_SOURCE})…")
        try:
            from cs2_vision_access.training.cs2_10k import materialize_cs2_10k_dataset

            mat = materialize_cs2_10k_dataset(
                data_dir,
                maps=cs2_10k_maps,
                max_shards=cs2_10k_max_shards,
                max_videos=cs2_10k_max_videos,
                frames_per_video=cs2_10k_frames_per_video,
                cache_dir=cs2_10k_cache_dir,
                holdout_video_fraction=float(cs2_10k_holdout_video_fraction),
            )
            notes.append(
                f"CS2-10k: maps={list(mat.maps)} shards={len(mat.shards)} "
                f"videos={mat.videos_processed} frames={mat.frames_written}"
            )
            notes.append(f"CS2-10k license: {_CS2_10K_LICENSE} (research / non-commercial)")
            dataset_license = _CS2_10K_LICENSE
            dataset_source = _CS2_10K_SOURCE
            n_images = deps.count_images(images_dir)
            n_val_holdout = (
                deps.count_images(data_dir / "images_val")
                if (data_dir / "images_val").is_dir()
                else 0
            )
            if n_val_holdout:
                notes.append(
                    f"CS2-10k video holdout: {n_val_holdout} frames under images_val/ "
                    f"(fraction={cs2_10k_holdout_video_fraction})"
                )
            n_images = n_images + n_val_holdout
            n_labels = deps.count_labels(labels_dir)
        except Exception as exc:  # noqa: BLE001
            notes.append(f"CS2-10k load failed: {exc}")
            raise FileNotFoundError(
                f"No images under {images_dir} and CS2-10k auto-load failed: {exc}\n"
                "Provide a dataset zip with images/, or install huggingface_hub "
                "and ensure network access to Hugging Face."
            ) from exc

    if n_images == 0:
        raise FileNotFoundError(
            f"No images under {images_dir}. Provide a dataset zip with images/, "
            "or enable use_cs2_10k=True (default) to pull RekaAI/CS2-10k."
        )

    return _ResolvedData(
        data_dir=data_dir,
        images_dir=images_dir,
        labels_dir=labels_dir,
        n_images=n_images,
        n_labels=n_labels,
        dataset_license=dataset_license,
        dataset_source=dataset_source,
    )


def resolve_remote_dataset_zip(
    explicit: str | Path | None = None,
    *,
    env_var: str = "CS2_DATASET_ZIP",
    search_roots: list[str | Path] | None = None,
) -> Path | None:
    """Resolve a dataset zip for notebooks without interactive upload."""
    import os as _os

    if explicit is not None and Path(explicit).is_file():
        return Path(explicit)
    env = _os.environ.get(env_var, "").strip()
    if env and Path(env).is_file():
        return Path(env)
    roots = search_roots or [
        "/kaggle/input",
        "/content",
        "/content/drive/MyDrive",
        ".",
        "data",
    ]
    return deps.find_dataset_zip(roots)
