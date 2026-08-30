"""Bakeoff execution and JSON report writing.

Separated from ``config.py`` so that config consumers do not depend on the
video-processing call chain.
"""

from __future__ import annotations

import json
import math
import os
import tempfile
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.application.ports.rendering import OutlineRenderer
from cs2_vision_access.application.ports.segmentation import Segmenter, create_segmenter
from cs2_vision_access.domain.outline import OutlineStyle
from cs2_vision_access.domain.video import VideoRunSummary
from cs2_vision_access.workflows.bakeoff.config import (
    _NUMERIC_RUN_FIELDS,
    _RUN_SUMMARY_FIELDS,
    DEFAULT_MAX_FRAMES,
    DEFAULT_OUTPUT,
    SCHEMA_VERSION,
    BakeoffBackendSpec,
    BakeoffError,
)


@dataclass(frozen=True)
class BakeoffResult:
    """Typed result returned by ``run_bakeoff`` before JSON serialisation."""

    schema_version: int
    input: str
    max_frames: int
    max_seconds: float | None
    runs: tuple[dict[str, object], ...]
    winner: None = None
    notes: str = (
        "Comparison only: latency and instance counts from the offline "
        "process_video path. No acceptance winner is declared."
    )


def _default_segmenter_factory(spec: BakeoffBackendSpec) -> Segmenter:
    return create_segmenter(
        spec.model,
        spec.manifest,
        backend=spec.backend,
        class_names=spec.class_names,
        confidence=spec.confidence,
        image_size=spec.image_size,
        device=spec.device,
    )


def row_from_summary(
    summary: VideoRunSummary,
    *,
    backend: str,
    model: Path,
    manifest: Path,
    eval_payload: Mapping[str, object] | None = None,
) -> dict[str, object]:
    """Project a VideoRunSummary into a bakeoff comparison row."""
    row: dict[str, object] = {
        "backend": backend,
        "model": str(model),
        "manifest": str(manifest),
    }
    for field in _RUN_SUMMARY_FIELDS:
        row[field] = getattr(summary, field)
    stamped = summary.segmenter_backend
    if stamped is not None and stamped != backend:
        row["segmenter_backend"] = stamped
    row["eval"] = dict(eval_payload) if eval_payload is not None else None
    _assert_row_finite(row)
    return row


def run_bakeoff(
    *,
    input_path: str | Path,
    backends: Sequence[BakeoffBackendSpec],
    max_frames: int = DEFAULT_MAX_FRAMES,
    max_seconds: float | None = None,
    output_path: str | Path | None = DEFAULT_OUTPUT,
    confidence: float | None = None,
    image_size: int | None = None,
    device: str | None = None,
    class_names: tuple[str, ...] | None = None,
    process_video_fn: Callable[..., VideoRunSummary] | None = None,
    segmenter_factory: Callable[..., Segmenter] | None = None,
    renderer: OutlineRenderer | None = None,
    renderer_factory: Callable[[OutlineStyle], OutlineRenderer] | None = None,
) -> dict[str, object]:
    """Run each backend sequentially on the same local video; return report dict.

    File-only: no live capture. Does not encode a winner. ``output_path`` of
    ``None`` skips writing to disk (report still returned).
    """
    if not backends:
        raise BakeoffError("backends list must not be empty")
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames <= 0:
        raise BakeoffError("max_frames must be a positive integer")
    if max_seconds is not None and (
        isinstance(max_seconds, bool)
        or not isinstance(max_seconds, (int, float))
        or not math.isfinite(float(max_seconds))
        or float(max_seconds) <= 0
    ):
        raise BakeoffError("max_seconds must be a finite positive number when set")

    source = Path(input_path)
    if process_video_fn is None:
        raise BakeoffError("a media processor must be supplied at the composition boundary")
    process = process_video_fn
    factory = segmenter_factory or _default_segmenter_factory
    if renderer is None and renderer_factory is None:
        raise BakeoffError("a rendering adapter must be supplied at the composition boundary")
    outline_renderer = renderer or renderer_factory(OutlineStyle())  # type: ignore[misc]

    runs: list[dict[str, object]] = []
    for spec in backends:
        effective = BakeoffBackendSpec(
            backend=spec.backend,
            model=spec.model,
            manifest=spec.manifest,
            confidence=confidence if confidence is not None else spec.confidence,
            image_size=image_size if image_size is not None else spec.image_size,
            device=device if device is not None else spec.device,
            class_names=class_names if class_names is not None else spec.class_names,
        )
        segmenter = factory(effective)
        summary = process(
            input_path=source,
            segmenter=segmenter,
            renderer=outline_renderer,
            output_path=None,
            display=False,
            realtime_playback=False,
            max_frames=max_frames,
            overwrite=False,
            max_seconds=max_seconds,
            cue_log_path=None,
        )
        runs.append(
            row_from_summary(
                summary,
                backend=effective.backend,
                model=effective.model,
                manifest=effective.manifest,
                eval_payload=None,
            )
        )

    report = BakeoffResult(
        schema_version=SCHEMA_VERSION,
        input=str(source),
        max_frames=max_frames,
        max_seconds=max_seconds,
        runs=tuple(runs),
    )
    report_dict = {
        "schema_version": report.schema_version,
        "input": report.input,
        "max_frames": report.max_frames,
        "max_seconds": report.max_seconds,
        "runs": list(report.runs),
        "winner": report.winner,
        "notes": report.notes,
    }
    _assert_report_finite(report_dict)

    if output_path is not None:
        write_bakeoff_json(report_dict, output_path)
    return report_dict


def write_bakeoff_json(payload: Mapping[str, object], path: str | Path) -> Path:
    """Atomically write a schema-versioned bakeoff report as sorted-key JSON."""
    if not isinstance(payload, Mapping):
        raise BakeoffError("bakeoff payload must be a mapping")
    if payload.get("schema_version") != SCHEMA_VERSION:
        raise BakeoffError(f"schema_version must be {SCHEMA_VERSION}")
    _assert_report_finite(payload)
    destination = Path(path)
    if destination.is_symlink():
        raise BakeoffError("output path must not be a symlink")
    if destination.suffix.lower() != ".json":
        raise BakeoffError("output path must end with .json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise BakeoffError("output parent must not be a symlink")
    text = json.dumps(dict(payload), indent=2, sort_keys=True, allow_nan=False) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_name = handle.name
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, destination)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return destination.resolve()


def _assert_row_finite(row: Mapping[str, object]) -> None:
    for key in _NUMERIC_RUN_FIELDS:
        value = row.get(key)
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise BakeoffError(f"run field {key!r} must be a finite number")
        if not math.isfinite(float(value)):
            raise BakeoffError(f"run field {key!r} must be finite (got {value!r})")
    reason = row.get("termination_reason")
    if not isinstance(reason, str) or not reason:
        raise BakeoffError("termination_reason must be a non-empty string")
    backend = row.get("backend")
    if not isinstance(backend, str) or not backend:
        raise BakeoffError("backend must be a non-empty string")


def _assert_report_finite(payload: Mapping[str, object]) -> None:
    version = payload.get("schema_version")
    if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
        raise BakeoffError(f"schema_version must be {SCHEMA_VERSION}")
    max_frames = payload.get("max_frames")
    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or max_frames <= 0:
        raise BakeoffError("max_frames must be a positive integer")
    max_seconds = payload.get("max_seconds")
    if max_seconds is not None:
        if isinstance(max_seconds, bool) or not isinstance(max_seconds, (int, float)):
            raise BakeoffError("max_seconds must be a finite number or null")
        if not math.isfinite(float(max_seconds)):
            raise BakeoffError("max_seconds must be finite when set")
    runs = payload.get("runs")
    if not isinstance(runs, list) or not runs:
        raise BakeoffError("runs must be a non-empty array")
    for index, run in enumerate(runs):
        if not isinstance(run, Mapping):
            raise BakeoffError(f"runs[{index}] must be an object")
        _assert_row_finite(run)
