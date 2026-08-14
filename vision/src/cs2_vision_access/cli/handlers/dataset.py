"""Dataset validation, audit, assemble, import, and boxes-to-masks subcommands."""

from __future__ import annotations

import argparse
from dataclasses import asdict
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.dataset import (
    DEFAULT_SPLIT as DEFAULT_IMPORT_SPLIT,
)
from cs2_vision_access.dataset import (
    LAYOUT_AUTO,
    SUPPORTED_LAYOUTS,
    audit_yolo_segmentation_dataset,
    import_box_dataset,
    validate_yolo_segmentation_dataset,
)
from cs2_vision_access.dataset_split import (
    assemble_dataset,
    build_split_plan,
    load_split_plan,
)
from cs2_vision_access.labeling import (
    DEFAULT_BACKEND as DEFAULT_BOXES_TO_MASKS_BACKEND,
)
from cs2_vision_access.labeling import (
    SUPPORTED_BACKENDS as BOXES_TO_MASKS_BACKENDS,
)
from cs2_vision_access.labeling import (
    boxes_to_masks,
    parse_class_map,
)
from cs2_vision_access.labeling import (
    parse_backend as parse_boxes_to_masks_backend,
)


def register_dataset_commands(subcommands: argparse._SubParsersAction) -> None:
    validate = subcommands.add_parser(
        "validate-dataset", help="validate YOLO polygon labels before training"
    )
    validate.add_argument("--root", type=Path, required=True)
    validate.add_argument("--class-count", type=int, required=True)
    validate.set_defaults(handler=_handle_validate)

    audit = subcommands.add_parser(
        "audit-dataset",
        help="audit structural integrity and train/val session leakage",
    )
    audit.add_argument("--root", type=Path, required=True)
    audit.add_argument("--class-count", type=int, required=True)
    audit.add_argument(
        "--sessions",
        type=Path,
        help="sessions.json mapping; defaults to <root>/sessions.json when present",
    )
    audit.add_argument(
        "--check-decode",
        action="store_true",
        help="attempt OpenCV image decode when OpenCV is installed",
    )
    audit.set_defaults(handler=_handle_audit)

    assemble = subcommands.add_parser(
        "assemble-dataset",
        help=(
            "copy staged session directories into YOLO train/val/test by whole "
            "session_id (never random-split adjacent frames)"
        ),
    )
    assemble.add_argument(
        "--staging-root",
        type=Path,
        required=True,
        help="directory of session folders (each session_id is one child directory)",
    )
    assemble.add_argument(
        "--output-root",
        type=Path,
        required=True,
        help="destination YOLO root (images/{split}/..., labels/{split}/...)",
    )
    assemble.add_argument(
        "--plan",
        type=Path,
        help='JSON object {"train": [...], "val": [...], "test": [...optional]}',
    )
    assemble.add_argument(
        "--train",
        action="append",
        default=[],
        metavar="SESSION_ID",
        help="session_id assigned to train; repeatable or comma-separated",
    )
    assemble.add_argument(
        "--val",
        action="append",
        default=[],
        metavar="SESSION_ID",
        help="session_id assigned to val; repeatable or comma-separated",
    )
    assemble.add_argument(
        "--test",
        action="append",
        default=[],
        metavar="SESSION_ID",
        help="optional session_id assigned to test; repeatable or comma-separated",
    )
    assemble.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing images/, labels/, and sessions.json under output-root",
    )
    assemble.set_defaults(handler=_handle_assemble)

    import_boxes = subcommands.add_parser(
        "import-box-dataset",
        help=(
            "import a local YOLO detection layout into data/staging/<session-id> "
            "for boxes-to-masks (no download; local path only)"
        ),
    )
    import_boxes.add_argument(
        "--source-root",
        type=Path,
        required=True,
        help="local YOLO-det tree (Ultralytics images/{split}+labels/{split}, flat, or pairs)",
    )
    import_boxes.add_argument(
        "--output-staging",
        type=Path,
        required=True,
        help="staging root; writes <output-staging>/<session-id>/",
    )
    import_boxes.add_argument(
        "--session-id",
        required=True,
        help="destination session directory name under output-staging",
    )
    import_boxes.add_argument(
        "--split",
        default=DEFAULT_IMPORT_SPLIT,
        help=f"YOLO layout split to import (default: {DEFAULT_IMPORT_SPLIT})",
    )
    import_boxes.add_argument(
        "--layout",
        choices=sorted(SUPPORTED_LAYOUTS),
        default=LAYOUT_AUTO,
        help="source layout: auto (default), yolo, or flat",
    )
    import_boxes.add_argument(
        "--max-images",
        type=int,
        default=None,
        metavar="N",
        help="optional cap on number of image/label pairs to import",
    )
    import_boxes.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing non-empty session directory",
    )
    import_boxes.add_argument(
        "--link",
        action="store_true",
        help="hardlink files when possible (falls back to copy across devices)",
    )
    import_boxes.set_defaults(handler=_handle_import_box_dataset)

    boxes_to_masks_cmd = subcommands.add_parser(
        "boxes-to-masks",
        help=(
            "convert YOLO detection boxes into draft segmentation polygons "
            "(always draft_pending; never ground truth)"
        ),
    )
    boxes_to_masks_cmd.add_argument(
        "--images-dir",
        type=Path,
        required=True,
        help="directory of source images (same-stem pairing with detection labels)",
    )
    boxes_to_masks_cmd.add_argument(
        "--labels-dir",
        type=Path,
        required=True,
        help="directory of YOLO detection labels (class_id x_c y_c w h)",
    )
    boxes_to_masks_cmd.add_argument(
        "--output-labels-dir",
        type=Path,
        required=True,
        help="directory for draft YOLO-seg polygon labels + draft_status.json",
    )
    boxes_to_masks_cmd.add_argument(
        "--backend",
        type=_parse_boxes_to_masks_backend,
        default=DEFAULT_BOXES_TO_MASKS_BACKEND,
        help=(
            "polygon backend: rectangle (default), ellipse, or sam "
            f"(optional; supported: {', '.join(sorted(BOXES_TO_MASKS_BACKENDS))})"
        ),
    )
    boxes_to_masks_cmd.add_argument(
        "--class-map",
        default=None,
        metavar="SPEC",
        help=(
            "source→output class map, e.g. ct=0,t=0,cthead=0,thead=0 or 0=0,1=0; "
            "default maps every source class to 0 (player)"
        ),
    )
    boxes_to_masks_cmd.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing draft label files and draft_status.json",
    )
    boxes_to_masks_cmd.set_defaults(handler=_handle_boxes_to_masks)


def _handle_validate(arguments: argparse.Namespace) -> int:
    result = validate_yolo_segmentation_dataset(arguments.root, arguments.class_count)
    _print_json(
        {
            "valid": result.is_valid,
            "summary": asdict(result.summary),
            "issues": [asdict(issue) for issue in result.issues],
        }
    )
    return 0 if result.is_valid else 1


def _handle_audit(arguments: argparse.Namespace) -> int:
    result = audit_yolo_segmentation_dataset(
        arguments.root,
        arguments.class_count,
        sessions_path=arguments.sessions,
        check_decode=arguments.check_decode,
    )
    _print_json(
        {
            "valid": result.is_valid,
            "summary": asdict(result.summary),
            "issues": [asdict(issue) for issue in result.issues],
        }
    )
    return 0 if result.is_valid else 1


def _handle_assemble(arguments: argparse.Namespace) -> int:
    has_lists = bool(arguments.train or arguments.val or arguments.test)
    if arguments.plan is not None and has_lists:
        raise ValueError("use either --plan or --train/--val/--test, not both")
    if arguments.plan is not None:
        plan = load_split_plan(arguments.plan)
    elif has_lists:
        plan = build_split_plan(
            train=arguments.train,
            val=arguments.val,
            test=arguments.test,
        )
    else:
        raise ValueError("assemble-dataset requires --plan or --train and --val")
    summary = assemble_dataset(
        arguments.staging_root,
        arguments.output_root,
        plan,
        overwrite=arguments.overwrite,
    )
    _print_json(summary.as_dict())
    return 0


def _handle_import_box_dataset(arguments: argparse.Namespace) -> int:
    summary = import_box_dataset(
        arguments.source_root,
        arguments.output_staging,
        arguments.session_id,
        split=arguments.split,
        layout=arguments.layout,
        max_images=arguments.max_images,
        overwrite=arguments.overwrite,
        link=arguments.link,
    )
    _print_json(summary.as_dict())
    return 0


def _parse_boxes_to_masks_backend(value: str) -> str:
    try:
        return parse_boxes_to_masks_backend(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def _handle_boxes_to_masks(arguments: argparse.Namespace) -> int:
    summary = boxes_to_masks(
        arguments.images_dir,
        arguments.labels_dir,
        arguments.output_labels_dir,
        backend=arguments.backend,
        class_map=parse_class_map(arguments.class_map),
        overwrite=arguments.overwrite,
    )
    _print_json(summary.as_dict())
    return 0
