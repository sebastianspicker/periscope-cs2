"""Shared training-data contracts (class maps, layouts, profiles, dataset.yaml).

Single source of truth for:

* Vombit / product class taxonomies and the collapse map
* Dataset layout variants (flat bootstrap vs session-split product)
* Ultralytics ``dataset.yaml`` writer
* Named hyperparameter profiles used by local CLI train, cloud notebooks,
  and ``training.train`` (self-train script)
* Profile lookup, hyperparameter resolution, label class-id checks, and
  class-name normalization helpers for remote / local training entry points

Layout note: held-out / product datasets that start flat are materialised into
the SESSION_SPLIT tree (``images/{train,val}`` + ``labels/{train,val}``) before
training. Remote writers may emit that layout directly; no separate layout
enum value is required for that path.
"""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path

# ---------------------------------------------------------------------------
# Class taxonomies
# ---------------------------------------------------------------------------

# Vombit detector taxonomy (ct / heads / t).
VOMBIT_CLASSES: dict[int, str] = {
    0: "ct",
    1: "ct_head",
    2: "t",
    3: "t_head",
}

# Collapsed product taxonomy used when all Vombit classes map to one player id.
PRODUCT_CLASSES: dict[int, str] = {
    0: "player",
}

# Source (Vombit) id → destination (product) id for --collapse-to-player.
VOMBIT_TO_PLAYER: dict[int, int] = {
    0: 0,
    1: 0,
    2: 0,
    3: 0,
}


# ---------------------------------------------------------------------------
# Layouts
# ---------------------------------------------------------------------------


class Layout(StrEnum):
    """Dataset directory layouts for Ultralytics yaml generation.

    After materialise / held-out split, product data uses SESSION_SPLIT
    (``images/train``, ``images/val``). Flat bootstrap is for early prepare
    batches where train and val share a single ``images/`` tree.
    """

    # Flat prepare/batch bootstrap: images/ + labels/ (train and val same dir).
    FLAT_BOOTSTRAP = "flat_bootstrap"
    # Product session-split: images/{train,val} + labels/{train,val}.
    SESSION_SPLIT = "session_split"


_LAYOUT_SPLITS: dict[Layout, tuple[str, str]] = {
    Layout.FLAT_BOOTSTRAP: ("images", "images"),
    Layout.SESSION_SPLIT: ("images/train", "images/val"),
}


# ---------------------------------------------------------------------------
# Hyperparameter profiles
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class TrainProfile:
    """Named hyperparameter bundle for a training path."""

    name: str
    epochs: int
    batch: int
    image_size: int
    base_model: str = "yolo11n-seg.pt"
    lr0: float = 0.001
    patience: int = 50
    description: str = ""


# Named profiles (SSOT for local.py, cloud.py, train.py).
PROFILES: dict[str, TrainProfile] = {
    "smoke": TrainProfile(
        name="smoke",
        epochs=1,
        batch=1,
        image_size=640,
        base_model="yolo26n-seg.pt",
        description="Few-epoch plumbing check only",
    ),
    "local": TrainProfile(
        name="local",
        epochs=100,
        batch=-1,  # Ultralytics AutoBatch
        image_size=640,
        base_model="yolo26n-seg.pt",
        description="CLI cs2-vision train defaults (full-size local run)",
    ),
    "cloud_t4": TrainProfile(
        name="cloud_t4",
        epochs=150,
        batch=16,  # VRAM-safe on free-tier T4 / Colab
        image_size=416,
        base_model="yolo11n-seg.pt",
        lr0=0.001,
        patience=50,
        description="Colab T4 / Kaggle / HF Space free-tier defaults",
    ),
    "self_train": TrainProfile(
        name="self_train",
        epochs=150,
        batch=16,
        image_size=416,
        base_model="yolo11n-seg.pt",
        lr0=0.001,
        patience=50,
        description="training.train module (pseudo-label self-train script)",
    ),
}


def resolve_profile(name: str) -> TrainProfile:
    """Look up a named :class:`TrainProfile` in :data:`PROFILES`.

    Args:
        name: Profile key (e.g. ``"cloud_t4"``, ``"local"``, ``"smoke"``).

    Returns:
        The frozen profile dataclass.

    Raises:
        ValueError: Unknown profile name (message lists known keys).
    """
    try:
        return PROFILES[name]
    except KeyError:
        known = ", ".join(sorted(PROFILES))
        raise ValueError(f"unknown train profile: {name!r}; known profiles: {known}") from None


def resolve_train_hyperparameters(
    *,
    profile: str | None = None,
    epochs: int | None = None,
    batch: int | None = None,
    image_size: int | None = None,
    base_model: str | None = None,
    lr0: float | None = None,
    patience: int | None = None,
) -> dict[str, object]:
    """Resolve training hyperparameters from a named profile plus overrides.

    When ``profile`` is ``None``, the package default is ``"cloud_t4"``
    (remote / free-tier oriented). Explicit non-``None`` keyword arguments
    always override the selected profile.

    Returns a plain dict with keys:
    ``epochs``, ``batch``, ``image_size``, ``base_model``, ``lr0``,
    ``patience``, ``profile_name``.

    Note:
        :func:`cs2_vision_access.workflows.training.local.resolve_train_hyperparameters`
        is a separate CLI helper (smoke + local defaults → tuple). This
        contracts API is the SSOT dict form for remote / shared callers.
    """
    profile_name = "cloud_t4" if profile is None else profile
    base = resolve_profile(profile_name)
    return {
        "epochs": epochs if epochs is not None else base.epochs,
        "batch": batch if batch is not None else base.batch,
        "image_size": image_size if image_size is not None else base.image_size,
        "base_model": base_model if base_model is not None else base.base_model,
        "lr0": lr0 if lr0 is not None else base.lr0,
        "patience": patience if patience is not None else base.patience,
        "profile_name": base.name,
    }


def assert_label_class_ids_compatible(
    labels_dir: Path,
    class_names: Mapping[int, str],
    *,
    max_files: int = 500,
    recursive: bool = False,
) -> list[str]:
    """Ensure YOLO label class ids are keys of ``class_names``.

    Scans up to ``max_files`` ``*.txt`` files under ``labels_dir``. By default
    only the top-level directory is scanned; set ``recursive=True`` to walk
    the tree (e.g. ``labels/train``, ``labels/val``).

    Empty files are skipped and reported as soft warnings. Blank lines inside
    non-empty files are ignored.

    Args:
        labels_dir: Directory containing YOLO ``.txt`` label files.
        class_names: Allowed class id → name map.
        max_files: Cap on files inspected (default 500).
        recursive: When True, ``rglob("*.txt")``; otherwise ``glob("*.txt")``.

    Returns:
        Warning strings (e.g. empty label files). Hard incompatibilities raise.

    Raises:
        ValueError: Directory missing, non-integer class tokens, or any class
            id not present in ``class_names``.
    """
    root = Path(labels_dir)
    if not root.is_dir():
        raise ValueError(f"labels directory does not exist: {root}")

    allowed = set(class_names.keys())
    pattern_iter = root.rglob("*.txt") if recursive else root.glob("*.txt")
    label_files = sorted(p for p in pattern_iter if p.is_file() and not p.is_symlink())
    if max_files >= 0:
        label_files = label_files[:max_files]

    warnings: list[str] = []
    bad: list[str] = []

    for path in label_files:
        try:
            rel = path.relative_to(root).as_posix()
        except ValueError:
            rel = path.name
        text = path.read_text(encoding="utf-8", errors="replace")
        if not text.strip():
            warnings.append(f"empty label file: {rel}")
            continue
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            line = raw_line.strip()
            if not line:
                continue
            token = line.split()[0]
            try:
                class_id = int(token)
            except ValueError:
                bad.append(f"{rel}:{line_no}: non-integer class id {token!r}")
                continue
            if class_id not in allowed:
                bad.append(
                    f"{rel}:{line_no}: class id {class_id} "
                    f"not in class_names keys {sorted(allowed)}"
                )

    if bad:
        preview = "; ".join(bad[:8])
        more = f" (+{len(bad) - 8} more)" if len(bad) > 8 else ""
        raise ValueError(
            f"label class ids incompatible with class_names ({len(bad)} issue(s)): {preview}{more}"
        )
    return warnings


def assert_label_class_ids_compatible_tree(
    labels_dir: Path,
    class_names: Mapping[int, str],
    *,
    max_files: int = 500,
) -> list[str]:
    """Recursive form of :func:`assert_label_class_ids_compatible`."""
    return assert_label_class_ids_compatible(
        labels_dir, class_names, max_files=max_files, recursive=True
    )


def normalize_class_names(
    classes: Mapping[int, str] | Mapping[str, str] | None,
    *,
    default: str = "product",
) -> dict[int, str]:
    """Normalise a class id → name map for training contracts.

    * ``None`` → :data:`PRODUCT_CLASSES` when ``default=="product"``, else
      :data:`VOMBIT_CLASSES`.
    * String keys that are decimal integers (e.g. ``"0"``) become ``int`` keys.
    * Contiguous non-negative ids starting at 0 are preferred for Ultralytics
      yaml, but non-contiguous maps are accepted.

    Args:
        classes: Raw class map, or ``None`` to use the taxonomy default.
        default: ``"product"`` selects product taxonomy when ``classes`` is
            ``None``; any other value selects Vombit.

    Returns:
        ``dict[int, str]`` with stripped names.

    Raises:
        ValueError: Invalid keys or empty names.
    """
    if classes is None:
        if default == "product":
            return dict(PRODUCT_CLASSES)
        return dict(VOMBIT_CLASSES)

    out: dict[int, str] = {}
    for key, value in classes.items():
        if isinstance(key, bool):
            raise ValueError("class name keys must be non-negative integers")
        if isinstance(key, int):
            class_id = key
        elif isinstance(key, str) and key.isascii() and key.isdecimal():
            class_id = int(key)
        else:
            raise ValueError(f"class name keys must be non-negative integers, got {key!r}")
        if class_id < 0:
            raise ValueError(f"class name keys must be non-negative, got {class_id}")
        if not isinstance(value, str) or not value.strip():
            raise ValueError("class names must be non-empty strings")
        out[class_id] = value.strip()
    return out


# ---------------------------------------------------------------------------
# dataset.yaml writer
# ---------------------------------------------------------------------------


def write_dataset_yaml(
    output_dir: Path | str,
    *,
    layout: Layout | str = Layout.FLAT_BOOTSTRAP,
    classes: Mapping[int, str] | None = None,
    portable_path: bool = False,
) -> Path:
    """Write Ultralytics ``dataset.yaml`` under ``output_dir``.

    Args:
        output_dir: Dataset root (contains ``images/`` and ``labels/``).
        layout: :class:`Layout` (or its string value). Defaults to flat bootstrap.
        classes: Class id → name map. Defaults to :data:`PRODUCT_CLASSES`.
        portable_path: When True, write ``path: .`` for zip/bundle portability;
            otherwise write an absolute path to ``output_dir``.

    Returns:
        Path to the written ``dataset.yaml``.

    Raises:
        ValueError: Unknown layout.
    """
    if isinstance(layout, str):
        try:
            layout = Layout(layout)
        except ValueError as exc:
            raise ValueError(f"unknown dataset layout: {layout!r}") from exc
    if layout not in _LAYOUT_SPLITS:
        raise ValueError(f"unknown dataset layout: {layout!r}")

    class_map = dict(classes) if classes is not None else dict(PRODUCT_CLASSES)
    train_rel, val_rel = _LAYOUT_SPLITS[layout]
    root = Path(output_dir)
    path_value = "." if portable_path else root.resolve().as_posix()
    names_block = "\n".join(f"  {idx}: {name}" for idx, name in sorted(class_map.items()))
    content = (
        f"# Ultralytics YOLO dataset config — auto-generated by cs2_train\n"
        f"path: {path_value}\n"
        f"train: {train_rel}\n"
        f"val: {val_rel}\n"
        f"\n"
        f"nc: {len(class_map)}\n"
        f"names:\n"
        f"{names_block}\n"
    )
    yaml_path = root / "dataset.yaml"
    root.mkdir(parents=True, exist_ok=True)
    yaml_path.write_text(content, encoding="utf-8")
    return yaml_path


def classes_for_class_map(class_map: dict[int, int] | None) -> dict[int, str]:
    """Choose yaml class names for a remap: collapsed → PRODUCT, else VOMBIT."""
    if class_map is None:
        return dict(VOMBIT_CLASSES)
    targets = set(class_map.values())
    if len(targets) == 1 and next(iter(targets)) == 0:
        return dict(PRODUCT_CLASSES)
    # Multi-target remap: keep VOMBIT names only for destination ids that appear.
    return {tid: VOMBIT_CLASSES.get(tid, f"class_{tid}") for tid in sorted(targets)}


__all__ = [
    "Layout",
    "PRODUCT_CLASSES",
    "PROFILES",
    "TrainProfile",
    "VOMBIT_CLASSES",
    "VOMBIT_TO_PLAYER",
    "assert_label_class_ids_compatible",
    "assert_label_class_ids_compatible_tree",
    "classes_for_class_map",
    "normalize_class_names",
    "resolve_profile",
    "resolve_train_hyperparameters",
    "write_dataset_yaml",
]
