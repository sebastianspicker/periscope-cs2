"""Evaluation subcommands: masks, negatives, temporal, comfort, export-predictions."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.cli.common import _print_json
from cs2_vision_access.evaluation import (
    DEFAULT_BOUNDARY_DILATION_PX,
    DEFAULT_IOU_THRESHOLD,
    evaluate_comfort_from_files,
    evaluate_masks_from_files,
    evaluate_negatives_from_files,
    evaluate_temporal_from_files,
    write_evaluation_json,
    write_metrics_json,
)
from cs2_vision_access.training.export_predictions import export_predictions


def register_eval_commands(subcommands: argparse._SubParsersAction) -> None:
    eval_masks = subcommands.add_parser(
        "eval-masks",
        help="compare cached mask predictions to YOLO polygon labels",
    )
    eval_masks.add_argument(
        "--predictions",
        type=Path,
        required=True,
        help="schema v1 JSON cache of pixel-space predicted polygons",
    )
    eval_masks.add_argument(
        "--dataset-root",
        type=Path,
        required=True,
        help="YOLO dataset root with labels/{split}/",
    )
    eval_masks.add_argument(
        "--split",
        default="test",
        help="label split under labels/; default: test",
    )
    eval_masks.add_argument(
        "--iou-threshold",
        type=float,
        default=DEFAULT_IOU_THRESHOLD,
        help=f"greedy match threshold; default: {DEFAULT_IOU_THRESHOLD}",
    )
    eval_masks.add_argument(
        "--boundary-dilation-px",
        type=int,
        default=DEFAULT_BOUNDARY_DILATION_PX,
        help=f"boundary F1 dilation radius; default: {DEFAULT_BOUNDARY_DILATION_PX}",
    )
    eval_masks.add_argument(
        "--output",
        type=Path,
        help="optional path for schema-versioned evaluation JSON",
    )
    eval_masks.set_defaults(handler=_handle_eval_masks)

    export_preds = subcommands.add_parser(
        "export-predictions",
        help=(
            "run an ONNX segmenter over dataset images and write schema v1 "
            "predictions JSON for eval-masks"
        ),
    )
    export_preds.add_argument(
        "--model",
        type=Path,
        required=True,
        help="path to ONNX model weights",
    )
    export_preds.add_argument(
        "--manifest",
        type=Path,
        required=True,
        help="path to model trust manifest JSON",
    )
    export_preds.add_argument(
        "--dataset-root",
        type=Path,
        required=True,
        help="YOLO dataset root with images/{split}/ or flat images/",
    )
    export_preds.add_argument(
        "--split",
        default="val",
        help=("image split under images/; default: val. Use all/flat for a flat images/ directory"),
    )
    export_preds.add_argument(
        "--output",
        type=Path,
        required=True,
        help="destination schema v1 predictions JSON path",
    )
    export_preds.add_argument(
        "--device",
        default="cpu",
        help="inference device (default: cpu); GPU: cuda:0, dml, …",
    )
    export_preds.add_argument(
        "--conf",
        type=float,
        default=0.25,
        help="segmenter confidence threshold; default: 0.25",
    )
    export_preds.add_argument(
        "--max-images",
        type=int,
        default=0,
        help="cap on images to process (0 = unlimited); default: 0",
    )
    export_preds.set_defaults(handler=_handle_export_predictions)

    eval_negatives = subcommands.add_parser(
        "eval-negatives",
        help=(
            "count false positives on expected-empty frames "
            "(expected_zero and/or empty YOLO labels) as FP per minute"
        ),
    )
    eval_negatives.add_argument(
        "--predictions",
        type=Path,
        required=True,
        help="schema v1 predictions JSON (use expected_zero and/or pair with labels)",
    )
    eval_negatives.add_argument(
        "--fps",
        type=float,
        required=True,
        help="frame rate used to convert frame counts to minutes",
    )
    eval_negatives.add_argument(
        "--dataset-root",
        type=Path,
        help="optional YOLO root; empty labels/{split}/*.txt select expected-empty frames",
    )
    eval_negatives.add_argument(
        "--split",
        default="test",
        help="label split under labels/ when --dataset-root is set; default: test",
    )
    eval_negatives.add_argument(
        "--output",
        type=Path,
        help="optional path for schema-versioned negatives evaluation JSON",
    )
    eval_negatives.set_defaults(handler=_handle_eval_negatives)

    eval_temporal = subcommands.add_parser(
        "eval-temporal",
        help=(
            "offline contour instability: frame-to-frame IoU match, "
            "centroid displacement, IoU drop, presence flicker"
        ),
    )
    eval_temporal.add_argument(
        "--sequence",
        type=Path,
        required=True,
        help=(
            "schema v1 JSON sequence: list or {frames:[{frame_index, width, height, "
            "predictions:[{class_id, polygon, confidence}]}]}"
        ),
    )
    eval_temporal.add_argument(
        "--iou-threshold",
        type=float,
        default=DEFAULT_IOU_THRESHOLD,
        help=f"greedy frame-to-frame match threshold; default: {DEFAULT_IOU_THRESHOLD}",
    )
    eval_temporal.add_argument(
        "--output",
        type=Path,
        help="optional path for schema-versioned temporal evaluation JSON",
    )
    eval_temporal.set_defaults(handler=_handle_eval_temporal)

    eval_comfort = subcommands.add_parser(
        "eval-comfort",
        help=(
            "comfort proxies: clutter fraction and local outer-stroke vs "
            "background contrast (≥3:1 sample fraction)"
        ),
    )
    eval_comfort.add_argument(
        "--frame",
        type=Path,
        required=True,
        help="source frame image (PNG/JPEG) in BGR decode order",
    )
    eval_comfort.add_argument(
        "--predictions",
        type=Path,
        required=True,
        help="schema v1 predictions JSON with absolute pixel polygons",
    )
    eval_comfort.add_argument(
        "--outer-color",
        default="#101010",
        help="dark outer stroke color (#RRGGBB); default: #101010",
    )
    eval_comfort.add_argument(
        "--inner-color",
        default="#F6FF00",
        help="bright inner stroke color (#RRGGBB); default: #F6FF00",
    )
    eval_comfort.add_argument(
        "--output",
        type=Path,
        help="optional path for schema-versioned comfort proxy JSON",
    )
    eval_comfort.set_defaults(handler=_handle_eval_comfort)


def _handle_eval_masks(arguments: argparse.Namespace) -> int:
    result = evaluate_masks_from_files(
        predictions_path=arguments.predictions,
        dataset_root=arguments.dataset_root,
        split=arguments.split,
        iou_threshold=arguments.iou_threshold,
        boundary_dilation_px=arguments.boundary_dilation_px,
    )
    payload = result.as_dict()
    if arguments.output is not None:
        write_evaluation_json(result, arguments.output)
        payload = {**payload, "output": str(arguments.output)}
    _print_json(payload)
    return 0


def _handle_export_predictions(arguments: argparse.Namespace) -> int:
    output = export_predictions(
        arguments.model,
        arguments.manifest,
        arguments.dataset_root,
        split=arguments.split,
        output_json=arguments.output,
        device=arguments.device,
        conf=arguments.conf,
        max_images=arguments.max_images,
    )
    _print_json(
        {
            "output": str(output),
            "split": arguments.split,
            "dataset_root": str(arguments.dataset_root),
        }
    )
    return 0


def _handle_eval_negatives(arguments: argparse.Namespace) -> int:
    result = evaluate_negatives_from_files(
        predictions_path=arguments.predictions,
        fps=arguments.fps,
        dataset_root=arguments.dataset_root,
        split=arguments.split,
    )
    payload = result.as_dict()
    if arguments.output is not None:
        write_metrics_json(payload, arguments.output)
        payload = {**payload, "output": str(arguments.output)}
    _print_json(payload)
    return 0


def _handle_eval_temporal(arguments: argparse.Namespace) -> int:
    result = evaluate_temporal_from_files(
        sequence_path=arguments.sequence,
        iou_threshold=arguments.iou_threshold,
    )
    payload = result.as_dict()
    if arguments.output is not None:
        write_metrics_json(payload, arguments.output)
        payload = {**payload, "output": str(arguments.output)}
    _print_json(payload)
    return 0


def _handle_eval_comfort(arguments: argparse.Namespace) -> int:
    result = evaluate_comfort_from_files(
        frame_path=arguments.frame,
        predictions_path=arguments.predictions,
        outer_color=arguments.outer_color,
        inner_color=arguments.inner_color,
    )
    payload = result.as_dict()
    if arguments.output is not None:
        write_metrics_json(payload, arguments.output)
        payload = {**payload, "output": str(arguments.output)}
    _print_json(payload)
    return 0
