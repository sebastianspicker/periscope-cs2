"""Import public YOLO detection layouts into staging sessions.

Converts a local YOLO-det tree into ``data/staging/<session_id>/`` with
``images/``, ``labels/`` (detection ``class x_c y_c w h``), and ``session.json``
ready for ``boxes-to-masks``. No network download — operators supply a local path.

Supported layouts:

1. ``yolo`` — Ultralytics-ish ``images/{split}/`` + ``labels/{split}/``
2. ``flat`` — ``images/`` + ``labels/`` side by side (no split subdirs required)
3. ``pairs`` (auto only) — single directory of same-stem image + ``.txt`` pairs
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.workflows.dataset.frames import (
    SCHEMA_VERSION,
    SESSION_FILENAME,
    FramePolicy,
    RightsPlaceholder,
    SessionProvenance,
    SessionProvenanceError,
)
from cs2_vision_access.workflows.dataset.import_copy import (
    copy_or_link,
    prepare_session_directory,
    write_session_json,
)
from cs2_vision_access.workflows.dataset.import_layout import (
    DEFAULT_SPLIT,
    LAYOUT_AUTO,
    LAYOUT_FLAT,
    LAYOUT_PAIRS,
    LAYOUT_YOLO,
    RESOLVED_LAYOUTS,
    SUPPORTED_LAYOUTS,
    ImportBoxDatasetError,
    collect_detection_pairs,
    detect_layout,
    normalise_split,
    require_existing_dir,
)
from cs2_vision_access.workflows.dataset.types import SPLIT_NAMES

IMPORT_RIGHTS_NOTE = "imported external box labels; audit license"

# Re-export layout symbols for ``from cs2_vision_access.workflows.dataset.import_boxes import ...``.
__all__ = [
    "DEFAULT_SPLIT",
    "IMPORT_RIGHTS_NOTE",
    "LAYOUT_AUTO",
    "LAYOUT_FLAT",
    "LAYOUT_PAIRS",
    "LAYOUT_YOLO",
    "RESOLVED_LAYOUTS",
    "SUPPORTED_LAYOUTS",
    "ImportBoxDatasetError",
    "ImportBoxDatasetSummary",
    "collect_detection_pairs",
    "detect_layout",
    "import_box_dataset",
]


@dataclass(frozen=True)
class ImportBoxDatasetSummary:
    source_root: str
    output_staging: str
    session_directory: str
    session_id: str
    layout: str
    split: str | None
    image_count: int
    label_count: int
    linked: bool
    session_path: str
    max_images: int | None

    def as_dict(self) -> dict[str, object]:
        return {
            "source_root": self.source_root,
            "output_staging": self.output_staging,
            "session_directory": self.session_directory,
            "session_id": self.session_id,
            "layout": self.layout,
            "split": self.split,
            "image_count": self.image_count,
            "label_count": self.label_count,
            "linked": self.linked,
            "session_path": self.session_path,
            "max_images": self.max_images,
            "schema_version": SCHEMA_VERSION,
        }


def import_box_dataset(
    source_root: str | Path,
    output_staging: str | Path,
    session_id: str,
    *,
    split: str = DEFAULT_SPLIT,
    layout: str = LAYOUT_AUTO,
    max_images: int | None = None,
    overwrite: bool = False,
    link: bool = False,
) -> ImportBoxDatasetSummary:
    """Copy (or hardlink) a local YOLO-det tree into a staging session directory."""
    source = require_existing_dir(source_root, "source root")
    staging_root = Path(output_staging)
    if staging_root.is_symlink():
        raise ImportBoxDatasetError("output staging must not be a symlink")
    resolved_session_id = _validate_session_id(session_id)
    if max_images is not None and (isinstance(max_images, bool) or max_images <= 0):
        raise ImportBoxDatasetError("max_images must be a positive integer when set")

    layout_name = layout.strip().lower() if isinstance(layout, str) else ""
    if layout_name not in SUPPORTED_LAYOUTS:
        raise ImportBoxDatasetError(
            f"layout must be one of {sorted(SUPPORTED_LAYOUTS)}; got {layout!r}"
        )

    resolved_layout = detect_layout(source) if layout_name == LAYOUT_AUTO else layout_name
    split_name = normalise_split(split) if resolved_layout == LAYOUT_YOLO else None
    if resolved_layout == LAYOUT_YOLO and split_name is None:
        raise ImportBoxDatasetError("yolo layout requires a non-empty --split")

    pairs = collect_detection_pairs(
        source,
        layout=resolved_layout,
        split=split_name or DEFAULT_SPLIT,
    )
    if not pairs:
        raise ImportBoxDatasetError(
            f"source has no image/label pairs under layout={resolved_layout!r}"
        )
    if max_images is not None:
        pairs = pairs[:max_images]
    if not pairs:
        raise ImportBoxDatasetError("max_images filtered out every pair")

    session_dir = staging_root / resolved_session_id
    prepare_session_directory(session_dir, overwrite=overwrite)

    images_out = session_dir / "images"
    labels_out = session_dir / "labels"
    images_out.mkdir(parents=True, exist_ok=True)
    labels_out.mkdir(parents=True, exist_ok=True)

    linked_any = False
    for image_path, label_path, relative_stem in pairs:
        relative = Path(relative_stem)
        dest_image = images_out / relative.with_suffix(image_path.suffix.lower())
        dest_label = labels_out / relative.with_suffix(".txt")
        dest_image.parent.mkdir(parents=True, exist_ok=True)
        dest_label.parent.mkdir(parents=True, exist_ok=True)
        if dest_image.is_symlink() or dest_label.is_symlink():
            raise ImportBoxDatasetError(
                f"refusing to write through symlink for stem {relative_stem!r}"
            )
        used_link = copy_or_link(image_path, dest_image, link=link)
        copy_or_link(label_path, dest_label, link=link)
        linked_any = linked_any or used_link

    image_count = len(pairs)
    provenance = SessionProvenance(
        schema_version=SCHEMA_VERSION,
        session_id=resolved_session_id,
        source_stem=source.name or "imported",
        rights=RightsPlaceholder(
            status="placeholder",
            source_identifier=str(source.resolve()),
            consent_record=IMPORT_RIGHTS_NOTE,
            redistribution="",
            retention_notes="",
        ),
        capture_notes=(
            f"import-box-dataset layout={resolved_layout}"
            + (f" split={split_name}" if split_name else "")
            + f"; {IMPORT_RIGHTS_NOTE}"
        ),
        frame_policy=FramePolicy(
            every_n_frames=1,
            max_saved_frames=image_count,
            decoded_frames=image_count,
            saved_frames=image_count,
        ),
    )
    try:
        SessionProvenance.from_mapping(provenance.as_json())
    except SessionProvenanceError as error:
        raise ImportBoxDatasetError(f"invalid session provenance: {error}") from error
    session_path = write_session_json(session_dir / SESSION_FILENAME, provenance)

    return ImportBoxDatasetSummary(
        source_root=str(source.resolve()),
        output_staging=str(staging_root.resolve()),
        session_directory=str(session_dir.resolve()),
        session_id=resolved_session_id,
        layout=resolved_layout,
        split=split_name,
        image_count=image_count,
        label_count=image_count,
        linked=linked_any,
        session_path=str(session_path),
        max_images=max_images,
    )


def _validate_session_id(session_id: str) -> str:
    if not isinstance(session_id, str) or not session_id.strip():
        raise ImportBoxDatasetError("session_id must be a non-empty string")
    cleaned = session_id.strip()
    if any(separator in cleaned for separator in ("/", "\\")):
        raise ImportBoxDatasetError(f"session_id must be a single path segment: {cleaned!r}")
    if cleaned in SPLIT_NAMES or cleaned in {".", ".."}:
        raise ImportBoxDatasetError(f"session_id is reserved: {cleaned!r}")
    return cleaned
