"""Autonomous loading of RekaAI/CS2-10k for self-train / remote notebooks.

Dataset: https://huggingface.co/datasets/RekaAI/CS2-10k

Published as WebDataset tar shards (~2 GB each) under ``data/<map>/`` with
``<uuid>.mp4`` + ``<uuid>.parquet`` pairs. This module downloads a **small
subset** of shards (not the full 60+ TB corpus), samples frames from the
videos, and writes a flat YOLO-style ``images/`` tree ready for bootstrap
labeling + iterative training.

License note: CS2-10k is **CC BY-NC 4.0** (research / non-commercial).
"""

from __future__ import annotations

import json
import random
import tarfile
import tempfile
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path

import cv2

CS2_10K_REPO_ID = "RekaAI/CS2-10k"
CS2_10K_URL = "https://huggingface.co/datasets/RekaAI/CS2-10k"

# Maps present in the HF release (data/<map>/).
CS2_10K_MAPS: tuple[str, ...] = (
    "ancient",
    "dust2",
    "inferno",
    "mirage",
    "nuke",
    "overpass",
    "train",
)

DEFAULT_MAP = "mirage"
DEFAULT_MAX_SHARDS = 1
DEFAULT_MAX_VIDEOS = 24
DEFAULT_FRAMES_PER_VIDEO = 12
DEFAULT_SAMPLE_EVERY_N = 48  # ~1 fps at 48 fps source

_HOLDOUT_JSON_NAME = "cs2_10k_holdout_videos.json"


class Cs210kError(RuntimeError):
    """CS2-10k download or materialization failed."""


class _ReproducibleDatasetRng(random.Random):
    """Non-cryptographic RNG reserved for reproducible dataset sampling."""


@dataclass(frozen=True)
class Cs210kMaterializeReport:
    """Summary of frames extracted from CS2-10k shards."""

    repo_id: str
    maps: tuple[str, ...]
    shards: tuple[str, ...]
    videos_processed: int
    frames_written: int
    output_dir: str
    cache_dir: str


def _require_hub() -> tuple[Callable[..., str], Callable[..., list[str]]]:
    try:
        from huggingface_hub import hf_hub_download, list_repo_files
    except ImportError as error:
        raise Cs210kError(
            "huggingface_hub is required to load CS2-10k. Install with: pip install huggingface_hub"
        ) from error
    return hf_hub_download, list_repo_files


def list_map_shards(
    map_name: str = DEFAULT_MAP,
    *,
    repo_id: str = CS2_10K_REPO_ID,
    revision: str | None = None,
) -> list[str]:
    """Return sorted repo-relative paths of ``.tar`` shards for a map."""
    map_name = map_name.strip().lower()
    if map_name not in CS2_10K_MAPS:
        raise Cs210kError(f"unknown map {map_name!r}; expected one of {CS2_10K_MAPS}")
    _, list_repo_files = _require_hub()
    prefix = f"data/{map_name}/"
    try:
        files = list_repo_files(repo_id, repo_type="dataset", revision=revision)
    except Exception as error:  # noqa: BLE001
        raise Cs210kError(f"could not list {repo_id}: {error}") from error
    shards = sorted(f for f in files if f.startswith(prefix) and f.endswith(".tar"))
    if not shards:
        raise Cs210kError(f"no .tar shards under {prefix} in {repo_id}")
    return shards


def download_shards(
    *,
    maps: Sequence[str] = (DEFAULT_MAP,),
    max_shards: int = DEFAULT_MAX_SHARDS,
    cache_dir: str | Path | None = None,
    repo_id: str = CS2_10K_REPO_ID,
    revision: str | None = None,
) -> list[Path]:
    """Download up to *max_shards* tar files (round-robin across *maps*)."""
    hf_hub_download, _ = _require_hub()
    cache = Path(cache_dir) if cache_dir else Path.home() / ".cache" / "cs2-10k"
    cache.mkdir(parents=True, exist_ok=True)

    selected: list[str] = []
    # Round-robin one shard per map until max_shards reached.
    map_lists = [list_map_shards(m, repo_id=repo_id, revision=revision) for m in maps]
    idx = 0
    while len(selected) < max_shards:
        progress = False
        for shards in map_lists:
            if idx < len(shards) and len(selected) < max_shards:
                selected.append(shards[idx])
                progress = True
        if not progress:
            break
        idx += 1

    if not selected:
        raise Cs210kError("no shards selected for download")

    local_paths: list[Path] = []
    for rel in selected:
        print(f"Downloading CS2-10k shard: {rel} (~2 GB)…")
        try:
            path = hf_hub_download(
                repo_id=repo_id,
                filename=rel,
                repo_type="dataset",
                revision=revision,
                local_dir=str(cache),
                local_dir_use_symlinks=False,
            )
        except TypeError:
            # Older huggingface_hub API
            path = hf_hub_download(
                repo_id=repo_id,
                filename=rel,
                repo_type="dataset",
                revision=revision,
                cache_dir=str(cache),
            )
        except Exception as error:  # noqa: BLE001
            raise Cs210kError(f"download failed for {rel}: {error}") from error
        local_paths.append(Path(path))
        print(f"  → {path}")
    return local_paths


def _merge_holdout_json(
    output_dir: Path,
    *,
    holdout_videos: list[str],
    holdout_frames: list[str],
    holdout_video_fraction: float,
) -> None:
    """Append holdout video/frame stems into ``cs2_10k_holdout_videos.json``.

    The file documents videos reserved for validation (and the frame stems
    extracted from them). Useful when holdout frames are mixed under
    ``images/`` or written to ``images_val/``.
    """
    path = output_dir / _HOLDOUT_JSON_NAME
    if path.is_file():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            data = {}
    else:
        data = {}
    videos = list(data.get("holdout_videos", []))
    frames = list(data.get("holdout_frames", []))
    videos.extend(holdout_videos)
    frames.extend(holdout_frames)
    # Deduplicate while preserving order.
    data["holdout_videos"] = list(dict.fromkeys(videos))
    data["holdout_frames"] = list(dict.fromkeys(frames))
    data["holdout_video_fraction"] = holdout_video_fraction
    data["note"] = (
        "Video stems held out for validation. Frame stems listed under "
        "holdout_frames were extracted from those videos (images_val/ when present)."
    )
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def _select_holdout_names(
    selected: list[str],
    fraction: float,
    rng: _ReproducibleDatasetRng,
) -> set[str]:
    """Pick a deterministic holdout subset of video member names."""
    if fraction <= 0.0 or not selected:
        return set()
    n = len(selected)
    n_hold = int(n * fraction)
    if fraction > 0.0 and n_hold == 0 and n >= 2:
        n_hold = 1
    if n_hold >= n and n > 1:
        n_hold = n - 1
    if n_hold <= 0:
        return set()
    # Sample without replacement from a copy so order of *selected* is stable.
    pool = list(selected)
    rng.shuffle(pool)
    return set(pool[:n_hold])


def extract_frames_from_tar(
    tar_path: str | Path,
    output_dir: str | Path,
    *,
    max_videos: int = DEFAULT_MAX_VIDEOS,
    frames_per_video: int = DEFAULT_FRAMES_PER_VIDEO,
    sample_every_n: int = DEFAULT_SAMPLE_EVERY_N,
    start_frame_index: int = 0,
    jpeg_quality: int = 90,
    shuffle_videos: bool = True,
    seed: int = 42,
    random_offset: bool = True,
    holdout_video_fraction: float = 0.0,
) -> tuple[int, int]:
    """Stream mp4s from a WebDataset tar and sample frames to ``images/``.

    Parameters
    ----------
    shuffle_videos:
        If True, shuffle mp4 members with the reproducible dataset RNG before taking
        the first ``max_videos``.
    seed:
        RNG seed for video shuffle and per-video start-frame offsets.
    random_offset:
        If True, for each video pick a start offset in ``[0, sample_every_n)``
        so sampling is not always locked to early frame indices.
    holdout_video_fraction:
        If > 0, hold out that fraction of selected videos for validation.
        Holdout frames go under ``images_val/``; video/frame stems are recorded
        in ``cs2_10k_holdout_videos.json``.

    Returns ``(videos_processed, frames_written)``.
    """
    tar_path = Path(tar_path)
    output_dir = Path(output_dir)
    images_dir = output_dir / "images"
    labels_dir = output_dir / "labels"
    images_dir.mkdir(parents=True, exist_ok=True)
    labels_dir.mkdir(parents=True, exist_ok=True)

    if not tar_path.is_file():
        raise Cs210kError(f"tar not found: {tar_path}")

    videos_done = 0
    frames_written = 0
    frame_index = int(start_frame_index)
    step = max(1, int(sample_every_n))
    # This sampling is intentionally reproducible: callers persist and test the seed.
    rng = _ReproducibleDatasetRng(seed)

    holdout_video_stems: list[str] = []
    holdout_frame_stems: list[str] = []

    with tarfile.open(tar_path, "r:*") as tf:
        # Collect all mp4 members first so we can shuffle before processing.
        mp4_members: list[tarfile.TarInfo] = []
        for member in tf.getmembers():
            if not member.isfile():
                continue
            name = member.name.replace("\\", "/")
            if name.lower().endswith(".mp4"):
                # Normalize name for consistent stems / holdout keys.
                member.name = name
                mp4_members.append(member)

        if shuffle_videos:
            rng.shuffle(mp4_members)

        selected = mp4_members[: max(0, int(max_videos))]
        holdout_names = _select_holdout_names(
            [m.name for m in selected],
            float(holdout_video_fraction),
            rng,
        )
        if holdout_names:
            (output_dir / "images_val").mkdir(parents=True, exist_ok=True)

        for member in selected:
            extracted = tf.extractfile(member)
            if extracted is None:
                continue
            raw = extracted.read()
            if not raw:
                continue

            stem = Path(member.name).stem
            is_holdout = member.name in holdout_names
            dest_dir = (output_dir / "images_val") if is_holdout else images_dir

            # Write temp mp4 for OpenCV
            with tempfile.NamedTemporaryFile(suffix=".mp4", delete=False) as tmp:
                tmp.write(raw)
                tmp_path = tmp.name
            try:
                cap = cv2.VideoCapture(tmp_path)
                if not cap.isOpened():
                    continue
                offset = rng.randrange(step) if random_offset else 0
                local_idx = 0
                taken = 0
                while taken < frames_per_video:
                    ok, frame = cap.read()
                    if not ok or frame is None:
                        break
                    if local_idx >= offset and (local_idx - offset) % step == 0:
                        out_path = dest_dir / f"frame_{frame_index:08d}.jpg"
                        cv2.imwrite(
                            str(out_path),
                            frame,
                            [cv2.IMWRITE_JPEG_QUALITY, int(jpeg_quality)],
                        )
                        if is_holdout:
                            holdout_frame_stems.append(out_path.stem)
                        frame_index += 1
                        frames_written += 1
                        taken += 1
                    local_idx += 1
                cap.release()
                videos_done += 1
                if is_holdout:
                    holdout_video_stems.append(stem)
                if videos_done % 5 == 0:
                    print(
                        f"  CS2-10k: {videos_done} videos, "
                        f"{frames_written} frames from {tar_path.name}"
                    )
            finally:
                Path(tmp_path).unlink(missing_ok=True)

    if holdout_video_fraction > 0.0:
        _merge_holdout_json(
            output_dir,
            holdout_videos=holdout_video_stems,
            holdout_frames=holdout_frame_stems,
            holdout_video_fraction=float(holdout_video_fraction),
        )

    return videos_done, frames_written


def materialize_cs2_10k_dataset(
    output_dir: str | Path,
    *,
    maps: Sequence[str] = (DEFAULT_MAP,),
    max_shards: int = DEFAULT_MAX_SHARDS,
    max_videos: int = DEFAULT_MAX_VIDEOS,
    frames_per_video: int = DEFAULT_FRAMES_PER_VIDEO,
    sample_every_n: int = DEFAULT_SAMPLE_EVERY_N,
    cache_dir: str | Path | None = None,
    repo_id: str = CS2_10K_REPO_ID,
    shuffle_videos: bool = True,
    seed: int = 42,
    random_offset: bool = True,
    holdout_video_fraction: float = 0.0,
) -> Cs210kMaterializeReport:
    """Download a few CS2-10k shards and sample frames into *output_dir*.

    Shards are selected round-robin across *maps* for map balance. Sampling
    supports deterministic shuffle, per-video random start offsets, and an
    optional video holdout list written to ``cs2_10k_holdout_videos.json``
    (holdout frames under ``images_val/``).

    Does **not** create labels (call bootstrap / teacher labeling next).
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    cache = Path(cache_dir) if cache_dir else Path.home() / ".cache" / "cs2-10k"

    print(
        f"Loading CS2-10k from {CS2_10K_URL}\n"
        f"  maps={list(maps)} max_shards={max_shards} "
        f"max_videos={max_videos} frames/video={frames_per_video}"
    )
    print("  License: CC BY-NC 4.0 (research / non-commercial only)")

    shards = download_shards(
        maps=maps,
        max_shards=max_shards,
        cache_dir=cache,
        repo_id=repo_id,
    )

    total_videos = 0
    total_frames = 0
    frame_cursor = 0
    for shard_i, shard in enumerate(shards):
        # Continue frame indices across shards
        existing = (
            sorted((output_dir / "images").glob("frame_*.jpg"))
            if (output_dir / "images").is_dir()
            else []
        )
        existing_val = (
            sorted((output_dir / "images_val").glob("frame_*.jpg"))
            if (output_dir / "images_val").is_dir()
            else []
        )
        all_existing = sorted(existing + existing_val, key=lambda p: p.name)
        if all_existing:
            try:
                frame_cursor = int(all_existing[-1].stem.split("_")[1]) + 1
            except (IndexError, ValueError):
                frame_cursor = total_frames
        # Per-shard seed so multi-shard runs diversify while staying deterministic.
        shard_seed = int(seed) + shard_i * 1_000_003
        v, f = extract_frames_from_tar(
            shard,
            output_dir,
            max_videos=max_videos,
            frames_per_video=frames_per_video,
            sample_every_n=sample_every_n,
            start_frame_index=frame_cursor,
            shuffle_videos=shuffle_videos,
            seed=shard_seed,
            random_offset=random_offset,
            holdout_video_fraction=holdout_video_fraction,
        )
        total_videos += v
        total_frames += f
        frame_cursor += f

    if total_frames == 0:
        raise Cs210kError(
            "no frames extracted from CS2-10k shards (check OpenCV video codecs / shard contents)"
        )

    print(f"✓ CS2-10k materialize: {total_videos} videos → {total_frames} frames")
    return Cs210kMaterializeReport(
        repo_id=repo_id,
        maps=tuple(maps),
        shards=tuple(str(s) for s in shards),
        videos_processed=total_videos,
        frames_written=total_frames,
        output_dir=str(output_dir.resolve()),
        cache_dir=str(cache.resolve()),
    )


__all__ = [
    "CS2_10K_MAPS",
    "CS2_10K_REPO_ID",
    "CS2_10K_URL",
    "Cs210kError",
    "Cs210kMaterializeReport",
    "DEFAULT_MAP",
    "download_shards",
    "extract_frames_from_tar",
    "list_map_shards",
    "materialize_cs2_10k_dataset",
]
