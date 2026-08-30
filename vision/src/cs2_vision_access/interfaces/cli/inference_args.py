"""Inference-related argparse helpers and segmenter construction."""

from __future__ import annotations

import argparse
from pathlib import Path

from cs2_vision_access.adapters.models.segmenters import (
    DEFAULT_SEGMENTER_BACKEND,
    SUPPORTED_SEGMENTER_BACKENDS,
    Segmenter,
    create_segmenter,
    normalize_segmenter_backend,
)


def _parse_segmenter_backend(value: str) -> str:
    normalized = normalize_segmenter_backend(value)
    if normalized not in SUPPORTED_SEGMENTER_BACKENDS:
        supported = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
        raise argparse.ArgumentTypeError(
            f"unknown or unsupported segmenter backend {value!r}; supported: {supported}"
        )
    return normalized


def _add_inference_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    supported_backends = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
    parser.add_argument(
        "--backend",
        type=_parse_segmenter_backend,
        default=DEFAULT_SEGMENTER_BACKEND,
        help=(
            f"segmentation backend; supported: {supported_backends}; "
            f"default: {DEFAULT_SEGMENTER_BACKEND}"
        ),
    )
    parser.add_argument("--class-name", action="append")
    parser.add_argument("--confidence", type=float, default=0.45)
    parser.add_argument("--image-size", type=int, default=640)
    parser.add_argument(
        "--device",
        default="cpu",
        help=(
            "Device for ONNX Runtime (default: cpu). GPU: cuda:0, dml, tensorrt, openvino, coreml"
        ),
    )
    parser.add_argument(
        "--max-frames",
        type=int,
        default=18_000,
        help="hard processing bound; default: 18000",
    )
    parser.add_argument(
        "--max-seconds",
        type=float,
        help="optional source-video duration bound for unattended preview or output",
    )


def _segmenter(arguments: argparse.Namespace) -> Segmenter:
    requested = tuple(arguments.class_name) if arguments.class_name is not None else None
    return create_segmenter(
        arguments.model,
        arguments.manifest,
        backend=arguments.backend,
        class_names=requested,
        confidence=arguments.confidence,
        image_size=arguments.image_size,
        device=arguments.device,
    )
