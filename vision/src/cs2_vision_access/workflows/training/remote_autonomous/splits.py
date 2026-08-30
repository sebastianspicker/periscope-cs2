"""Held-out split materialization and CS2-10k holdout promotion."""

from __future__ import annotations

import json
import random
from collections.abc import Mapping
from pathlib import Path

from cs2_vision_access.workflows.training import remote_autonomous_bindings as deps

from .fsutil import (
    _assert_labels_compatible,
    _ensure_dirs,
    _find_image_path,
    _link_or_copy,
    _list_image_stems,
)
from .models import (
    _CS2_10K_HOLDOUT_JSON,
    _DEFAULT_SPLIT_SEED,
    _HeldOutSplitResult,
)


class _ReproducibleSplitRng(random.Random):
    """Non-cryptographic RNG reserved for repeatable train/validation splits."""


def materialize_held_out_split(
    data_dir: str | Path,
    *,
    val_fraction: float = 0.2,
    seed: int = _DEFAULT_SPLIT_SEED,
) -> dict[str, list[str]]:
    """Hardlink/copy flat ``images/`` + ``labels/`` into train/val subdirs.

    Flat files are left in place so ``cloud.train`` preflight counts still work;
    Ultralytics is pointed at the split via :class:`Layout.SESSION_SPLIT`.

    Returns ``{"train": [stems...], "val": [stems...]}``.
    """
    if not 0.0 < float(val_fraction) < 1.0:
        raise ValueError("val_fraction must be in (0, 1)")

    data_dir = Path(data_dir)
    images_dir, labels_dir = _ensure_dirs(data_dir)
    stems = _list_image_stems(images_dir)
    if not stems:
        raise FileNotFoundError(f"No images under {images_dir} for held-out split")

    # The seed is a reproducibility contract for dataset split plans, not entropy.
    rng = _ReproducibleSplitRng(int(seed))
    ordered = list(stems)
    rng.shuffle(ordered)
    n = len(ordered)
    if n < 2:
        train_stems = list(ordered)
        val_stems = list(ordered)  # tiny set: val mirrors train (degraded)
    else:
        n_val = max(1, int(round(n * float(val_fraction))))
        if n_val >= n:
            n_val = n - 1
        val_stems = ordered[:n_val]
        train_stems = ordered[n_val:]

    for split_name, split_stems in (("train", train_stems), ("val", val_stems)):
        for stem in split_stems:
            img = _find_image_path(images_dir, stem)
            if img is not None:
                _link_or_copy(img, images_dir / split_name / img.name)
            lab = labels_dir / f"{stem}.txt"
            if lab.is_file():
                _link_or_copy(lab, labels_dir / split_name / lab.name)
                # Propagate revisable-pseudo sidecars (Ultralytics-safe) so
                # self-train can still overwrite_pseudo on the train split.
                pseudo = labels_dir / f"{stem}.txt.pseudo"
                if pseudo.is_file():
                    _link_or_copy(pseudo, labels_dir / split_name / pseudo.name)

    # Ensure empty split dirs exist even if a side has no labels yet.
    for split_name in ("train", "val"):
        (images_dir / split_name).mkdir(parents=True, exist_ok=True)
        (labels_dir / split_name).mkdir(parents=True, exist_ok=True)

    plan = {"train": list(train_stems), "val": list(val_stems)}
    plan_path = data_dir / "held_out_split.json"
    plan_path.write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
    print(
        f"✓ Held-out split: train={len(train_stems)} val={len(val_stems)} "
        f"(val_fraction={val_fraction}, seed={seed})"
    )
    return plan


def _load_cs2_10k_holdout_frame_stems(data_dir: Path) -> list[str]:
    """Return ordered holdout frame stems from ``images_val/`` and/or holdout JSON."""
    from cs2_vision_access.workflows.dataset.types import IMAGE_EXTENSIONS

    stems: list[str] = []
    seen: set[str] = set()
    images_val = data_dir / "images_val"
    if images_val.is_dir():
        for path in sorted(images_val.iterdir()):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS and path.stem not in seen:
                stems.append(path.stem)
                seen.add(path.stem)

    json_path = data_dir / _CS2_10K_HOLDOUT_JSON
    if json_path.is_file():
        try:
            payload = json.loads(json_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            payload = {}
        if isinstance(payload, dict):
            for raw in payload.get("holdout_frames", []) or []:
                stem = str(raw).strip()
                if stem and stem not in seen:
                    stems.append(stem)
                    seen.add(stem)
    return stems


def _link_stem_into_split(
    *,
    stem: str,
    images_src: Path,
    labels_src: Path,
    images_dst_dir: Path,
    labels_dst_dir: Path,
) -> None:
    """Hardlink/copy one stem's image + label (+ pseudo sidecar) into a split dir."""
    img = _find_image_path(images_src, stem)
    if img is not None:
        _link_or_copy(img, images_dst_dir / img.name)
    lab = labels_src / f"{stem}.txt"
    if lab.is_file():
        _link_or_copy(lab, labels_dst_dir / lab.name)
        pseudo = labels_src / f"{stem}.txt.pseudo"
        if pseudo.is_file():
            _link_or_copy(pseudo, labels_dst_dir / pseudo.name)


def promote_cs2_10k_holdout_to_session_split(
    data_dir: str | Path,
) -> dict[str, list[str]] | None:
    """Promote CS2-10k video holdout into ``images/val`` + ``labels/val``.

    Uses ``images_val/`` and/or ``cs2_10k_holdout_videos.json`` to identify val
    stems. Remaining flat ``images/`` stems become train. Does **not** randomly
    re-split holdout into train.

    Returns ``{"train": [...], "val": [...]}`` when holdout was found, else
    ``None`` (caller should fall back to :func:`materialize_held_out_split`).
    """
    data_dir = Path(data_dir)
    images_dir, labels_dir = _ensure_dirs(data_dir)
    val_stems = _load_cs2_10k_holdout_frame_stems(data_dir)
    if not val_stems:
        return None

    val_set = set(val_stems)
    # Flat pool only — never pull holdout stems into train.
    train_stems = [s for s in _list_image_stems(images_dir) if s not in val_set]
    images_val_dir = data_dir / "images_val"

    for stem in val_stems:
        # Prefer dedicated images_val/; fall back to flat images/ if mixed layout.
        img = None
        if images_val_dir.is_dir():
            img = _find_image_path(images_val_dir, stem)
        if img is None:
            img = _find_image_path(images_dir, stem)
        if img is not None:
            _link_or_copy(img, images_dir / "val" / img.name)
        lab = labels_dir / f"{stem}.txt"
        if lab.is_file():
            _link_or_copy(lab, labels_dir / "val" / lab.name)
            pseudo = labels_dir / f"{stem}.txt.pseudo"
            if pseudo.is_file():
                _link_or_copy(pseudo, labels_dir / "val" / pseudo.name)

    for stem in train_stems:
        _link_stem_into_split(
            stem=stem,
            images_src=images_dir,
            labels_src=labels_dir,
            images_dst_dir=images_dir / "train",
            labels_dst_dir=labels_dir / "train",
        )

    for split_name in ("train", "val"):
        (images_dir / split_name).mkdir(parents=True, exist_ok=True)
        (labels_dir / split_name).mkdir(parents=True, exist_ok=True)

    plan_out = {
        "train": list(train_stems),
        "val": list(val_stems),
        "source": "cs2_10k_video_holdout",
    }
    plan_path = data_dir / "held_out_split.json"
    plan_path.write_text(json.dumps(plan_out, indent=2) + "\n", encoding="utf-8")
    print(
        f"✓ CS2-10k video holdout → session_split: "
        f"train={len(train_stems)} val={len(val_stems)} "
        f"(holdout stems never re-split into train)"
    )
    return {"train": list(train_stems), "val": list(val_stems)}


def _phase_held_out_split(
    data_dir: Path,
    *,
    images_dir: Path,
    labels_dir: Path,
    class_map: Mapping[int, str],
    allow_leaky_val: bool,
    val_fraction: float,
    notes: list[str],
) -> _HeldOutSplitResult:
    """Promote CS2-10k holdout or materialize a random held-out val split."""
    use_split = not allow_leaky_val
    train_images_dir = images_dir
    train_labels_dir = labels_dir
    layout = deps.Layout.FLAT_BOOTSTRAP
    if use_split:
        holdout_plan = promote_cs2_10k_holdout_to_session_split(data_dir)
        if holdout_plan is not None:
            notes.append(
                f"CS2-10k video holdout → session_split "
                f"train={len(holdout_plan['train'])} val={len(holdout_plan['val'])} "
                f"(skipped random stem split)"
            )
        else:
            materialize_held_out_split(
                data_dir,
                val_fraction=val_fraction,
                seed=_DEFAULT_SPLIT_SEED,
            )
            notes.append(
                f"held-out val_fraction={val_fraction} layout=session_split "
                f"(self-train on images/train only)"
            )
        train_images_dir = images_dir / "train"
        train_labels_dir = labels_dir / "train"
        layout = deps.Layout.SESSION_SPLIT
        deps.write_dataset_yaml(
            data_dir,
            layout=deps.Layout.SESSION_SPLIT,
            classes=class_map,
            portable_path=False,
        )
        for w in _assert_labels_compatible(
            [labels_dir / "train", labels_dir / "val"],
            class_map,
        ):
            notes.append(f"label check: {w}")
        if deps.count_labels(train_labels_dir) == 0:
            raise RuntimeError(
                "No labels under labels/train after held-out split. "
                "Ensure bootstrap/gold labels exist for train stems."
            )
    else:
        deps.write_dataset_yaml(
            data_dir,
            layout=deps.Layout.FLAT_BOOTSTRAP,
            classes=class_map,
            portable_path=False,
        )
        notes.append("allow_leaky_val=True: flat train=val layout (legacy)")

    return _HeldOutSplitResult(
        train_images_dir=train_images_dir,
        train_labels_dir=train_labels_dir,
        layout=layout,
    )
