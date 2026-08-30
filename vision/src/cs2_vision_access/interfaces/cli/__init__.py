"""Command-line interface for the file-only research workflow."""

from __future__ import annotations

import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any

from cs2_vision_access.interfaces.cli.parser import build_parser
from cs2_vision_access.interfaces.cli.style_args import _outline_style

__all__ = ["main", "build_parser", "_outline_style"]


def main(argv: Sequence[str] | None = None) -> int:
    from cs2_vision_access.adapters.models.runtime.optimize import convert_to_fp16
    from cs2_vision_access.adapters.models.runtime.providers import recommend_device
    from cs2_vision_access.adapters.models.segmenters.factory import create_segmenter
    from cs2_vision_access.application.ports.model_runtime import register_model_runtime
    from cs2_vision_access.application.ports.segmentation import (
        Segmenter,
        register_segmenter_factory,
    )

    def registered_segmenter_factory(
        model_path: str | Path, manifest_path: str | Path, **kwargs: Any
    ) -> Segmenter:
        return create_segmenter(model_path, manifest_path, **kwargs)

    register_segmenter_factory(registered_segmenter_factory)
    register_model_runtime(recommend_device=recommend_device, convert_to_fp16=convert_to_fp16)
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        return int(arguments.handler(arguments))
    except (ValueError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
