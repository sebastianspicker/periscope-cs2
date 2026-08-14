"""register-model and train subcommands."""

from __future__ import annotations

import argparse
from dataclasses import asdict
from pathlib import Path

from cs2_vision_access.cli.common import (
    _load_classes_json,
    _parse_classes,
    _print_json,
)
from cs2_vision_access.model_manifest import create_manifest
from cs2_vision_access.training import (
    DEFAULT_BATCH,
    DEFAULT_EPOCHS,
    DEFAULT_IMAGE_SIZE,
    DEFAULT_PROJECT_DIRECTORY,
    DEFAULT_RUN_NAME,
    SMOKE_BATCH,
    SMOKE_EPOCHS,
    resolve_train_hyperparameters,
    train_and_export,
)


def register_model_commands(subcommands: argparse._SubParsersAction) -> None:
    register = subcommands.add_parser(
        "register-model", help="write a checksum manifest for a trusted local ONNX file"
    )
    register.add_argument("--model", type=Path, required=True)
    register.add_argument("--manifest", type=Path, required=True)
    register_classes = register.add_mutually_exclusive_group(required=True)
    register_classes.add_argument(
        "--class",
        dest="classes",
        action="append",
        metavar="ID=NAME",
    )
    register_classes.add_argument("--classes-json", type=Path)
    register.add_argument("--origin", required=True)
    register.add_argument("--license", dest="license_name", required=True)
    register.add_argument("--overwrite", action="store_true")
    register.set_defaults(handler=_handle_register)

    train = subcommands.add_parser(
        "train",
        help=(
            "fine-tune a YOLO segmenter and export a verified ONNX artifact "
            "(CS2 player class 0=player; not the COCO person plumbing baseline)"
        ),
    )
    train.add_argument("--dataset-yaml", type=Path, required=True)
    train.add_argument("--dataset-root", type=Path, required=True)
    train.add_argument(
        "--class",
        dest="classes",
        action="append",
        metavar="ID=NAME",
        help="class mapping (default: 0=player for CS2-specific training)",
    )
    train.add_argument("--base-model", default="yolo26n-seg.pt")
    train.add_argument("--base-model-origin", required=True)
    train.add_argument("--exported-model-license", required=True)
    train.add_argument("--allow-model-download", action="store_true")
    train.add_argument(
        "--smoke",
        action="store_true",
        help=(
            "few-epoch plumbing check (epochs=1, batch=1 unless overridden); "
            "does not produce a validated CS2 model"
        ),
    )
    train.add_argument(
        "--epochs",
        type=int,
        default=None,
        help=(f"training epochs (default: {DEFAULT_EPOCHS}, or {SMOKE_EPOCHS} with --smoke)"),
    )
    train.add_argument(
        "--image-size",
        type=int,
        default=None,
        help=f"square training/export size (default: {DEFAULT_IMAGE_SIZE})",
    )
    train.add_argument(
        "--batch",
        type=int,
        default=None,
        help=(
            f"batch size, -1 = AutoBatch (default: {DEFAULT_BATCH}, or {SMOKE_BATCH} with --smoke)"
        ),
    )
    train.add_argument(
        "--device",
        default="0",
        help="Ultralytics device string (default: 0; use cpu for smoke without GPU)",
    )
    train.add_argument(
        "--project-directory",
        type=Path,
        default=DEFAULT_PROJECT_DIRECTORY,
        help=(
            "Ultralytics project root for run artifacts "
            f"(default: {DEFAULT_PROJECT_DIRECTORY.as_posix()})"
        ),
    )
    train.add_argument(
        "--run-name",
        default=DEFAULT_RUN_NAME,
        help=(
            "run subdirectory under project-directory "
            f"(default: {DEFAULT_RUN_NAME}); weights/best.pt and ONNX export land here"
        ),
    )
    train.set_defaults(handler=_handle_train)


def _handle_register(arguments: argparse.Namespace) -> int:
    classes = (
        _load_classes_json(arguments.classes_json)
        if arguments.classes_json is not None
        else _parse_classes(arguments.classes)
    )
    path = create_manifest(
        arguments.model,
        arguments.manifest,
        classes=classes,
        origin=arguments.origin,
        license_name=arguments.license_name,
        overwrite=arguments.overwrite,
    )
    _print_json({"manifest": str(path)})
    return 0


def _handle_train(arguments: argparse.Namespace) -> int:
    classes = _parse_classes(arguments.classes or ["0=player"])
    epochs, batch, image_size = resolve_train_hyperparameters(
        smoke=arguments.smoke,
        epochs=arguments.epochs,
        batch=arguments.batch,
        image_size=arguments.image_size,
    )
    summary = train_and_export(
        dataset_yaml=arguments.dataset_yaml,
        dataset_root=arguments.dataset_root,
        class_names=classes,
        base_model=arguments.base_model,
        base_model_origin=arguments.base_model_origin,
        exported_model_license=arguments.exported_model_license,
        allow_model_download=arguments.allow_model_download,
        epochs=epochs,
        image_size=image_size,
        batch=batch,
        device=arguments.device,
        project_directory=arguments.project_directory,
        run_name=arguments.run_name,
    )
    _print_json(asdict(summary))
    return 0
