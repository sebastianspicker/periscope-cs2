"""Bakeoff configuration — dataclass, JSON loading, CLI parsing."""

from __future__ import annotations

import json
import math
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from cs2_vision_access.application.ports.segmentation import (
    SUPPORTED_SEGMENTER_BACKENDS,
    normalize_segmenter_backend,
)

SCHEMA_VERSION = 1
DEFAULT_MAX_FRAMES = 18_000
DEFAULT_OUTPUT = Path("artifacts/bakeoff.json")

# Fields copied from VideoRunSummary into each bakeoff run row.
_RUN_SUMMARY_FIELDS: tuple[str, ...] = (
    "frames_processed",
    "instances_predicted",
    "instances_outlined",
    "inference_ms_p50",
    "inference_ms_p95",
    "pipeline_ms_p50",
    "pipeline_ms_p95",
    "frame_budget_misses",
    "termination_reason",
)

_NUMERIC_RUN_FIELDS = frozenset(
    {
        "frames_processed",
        "instances_predicted",
        "instances_outlined",
        "inference_ms_p50",
        "inference_ms_p95",
        "pipeline_ms_p50",
        "pipeline_ms_p95",
        "frame_budget_misses",
    }
)


class BakeoffError(ValueError):
    """Bakeoff configuration or execution failed closed."""


@dataclass(frozen=True)
class BakeoffBackendSpec:
    """One backend entry: registered name + checksum-bound model/manifest."""

    backend: str
    model: Path
    manifest: Path
    confidence: float = 0.45
    image_size: int = 640
    device: str = "cpu"
    class_names: tuple[str, ...] | None = None

    def __post_init__(self) -> None:
        normalized = normalize_segmenter_backend(self.backend)
        if normalized not in SUPPORTED_SEGMENTER_BACKENDS:
            supported = ", ".join(sorted(SUPPORTED_SEGMENTER_BACKENDS))
            raise BakeoffError(
                f"unknown or unsupported segmenter backend {self.backend!r}; supported: {supported}"
            )
        object.__setattr__(self, "backend", normalized)
        model = Path(self.model)
        manifest = Path(self.manifest)
        object.__setattr__(self, "model", model)
        object.__setattr__(self, "manifest", manifest)
        if (
            isinstance(self.confidence, bool)
            or not isinstance(self.confidence, (int, float))
            or not math.isfinite(float(self.confidence))
            or not 0.0 <= float(self.confidence) <= 1.0
        ):
            raise BakeoffError("confidence must be a finite number in [0, 1]")
        object.__setattr__(self, "confidence", float(self.confidence))
        if (
            isinstance(self.image_size, bool)
            or not isinstance(self.image_size, int)
            or self.image_size <= 0
        ):
            raise BakeoffError("image_size must be a positive integer")
        if not isinstance(self.device, str) or not self.device.strip():
            raise BakeoffError("device must be a non-empty string")
        object.__setattr__(self, "device", self.device.strip())
        if self.class_names is not None:
            if not self.class_names or any(
                not isinstance(name, str) or not name.strip() for name in self.class_names
            ):
                raise BakeoffError(
                    "class_names must be a non-empty tuple of non-empty strings when provided"
                )
            object.__setattr__(
                self,
                "class_names",
                tuple(name.strip() for name in self.class_names),
            )


def load_bakeoff_config(path: str | Path) -> dict[str, object]:
    """Load a bakeoff JSON config (backends list + optional shared knobs)."""
    candidate = Path(path)
    if candidate.is_symlink():
        raise BakeoffError("bakeoff config path must not be a symlink")
    if not candidate.is_file() or candidate.suffix.lower() != ".json":
        raise BakeoffError("bakeoff config must be a regular local .json file")
    try:
        payload = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BakeoffError(f"could not read bakeoff config: {error}") from error
    if not isinstance(payload, dict):
        raise BakeoffError("bakeoff config root must be an object")
    return payload


def specs_from_config(
    payload: Mapping[str, object],
    *,
    base_directory: str | Path | None = None,
) -> tuple[BakeoffBackendSpec, ...]:
    """Parse ``backends`` from a config object into validated specs."""
    raw_backends = payload.get("backends")
    if raw_backends is None:
        raise BakeoffError("bakeoff config requires a 'backends' array")
    if not isinstance(raw_backends, list):
        raise BakeoffError("bakeoff config 'backends' must be an array")
    if not raw_backends:
        raise BakeoffError("backends list must not be empty")

    base = Path(base_directory) if base_directory is not None else None
    shared_confidence = payload.get("confidence", 0.45)
    shared_image_size = payload.get("image_size", 640)
    shared_device = payload.get("device", "cpu")
    shared_class_names = _coerce_class_names(payload.get("class_names"))

    specs: list[BakeoffBackendSpec] = []
    for index, entry in enumerate(raw_backends):
        if not isinstance(entry, dict):
            raise BakeoffError(f"backends[{index}] must be an object")
        backend = entry.get("backend")
        model = entry.get("model")
        manifest = entry.get("manifest")
        if not isinstance(backend, str) or not backend.strip():
            raise BakeoffError(f"backends[{index}].backend must be a non-empty string")
        if not isinstance(model, str) or not model.strip():
            raise BakeoffError(f"backends[{index}].model must be a non-empty string path")
        if not isinstance(manifest, str) or not manifest.strip():
            raise BakeoffError(f"backends[{index}].manifest must be a non-empty string path")
        model_path = Path(model)
        manifest_path = Path(manifest)
        if base is not None:
            if not model_path.is_absolute():
                model_path = base / model_path
            if not manifest_path.is_absolute():
                manifest_path = base / manifest_path
        confidence = entry.get("confidence", shared_confidence)
        image_size = entry.get("image_size", shared_image_size)
        device = entry.get("device", shared_device)
        raw_class_names = entry.get("class_names", shared_class_names)
        class_names = _coerce_class_names(raw_class_names)
        if isinstance(confidence, bool) or not isinstance(confidence, (int, float)):
            raise BakeoffError(f"backends[{index}].confidence must be a number in [0, 1]")
        if isinstance(image_size, bool) or not isinstance(image_size, int):
            raise BakeoffError(f"backends[{index}].image_size must be a positive integer")
        if not isinstance(device, str):
            raise BakeoffError(f"backends[{index}].device must be a string")
        specs.append(
            BakeoffBackendSpec(
                backend=backend,
                model=model_path,
                manifest=manifest_path,
                confidence=float(confidence),
                image_size=image_size,
                device=device,
                class_names=class_names,
            )
        )
    return tuple(specs)


def specs_from_cli_pairs(
    backends: Sequence[str],
    *,
    model_a: str | Path | None,
    manifest_a: str | Path | None,
    model_b: str | Path | None = None,
    manifest_b: str | Path | None = None,
    confidence: float = 0.45,
    image_size: int = 640,
    device: str = "cpu",
    class_names: tuple[str, ...] | None = None,
) -> tuple[BakeoffBackendSpec, ...]:
    """Map comma-list backends to model-a/manifest-a and model-b/manifest-b pairs."""
    names = [name.strip() for name in backends if name.strip()]
    if not names:
        raise BakeoffError("backends list must not be empty")
    if len(names) > 2:
        raise BakeoffError("CLI --backends supports at most two entries (use --config for more)")
    pairs: list[tuple[Path | None, Path | None]] = [
        (
            Path(model_a) if model_a is not None else None,
            Path(manifest_a) if manifest_a is not None else None,
        ),
        (
            Path(model_b) if model_b is not None else None,
            Path(manifest_b) if manifest_b is not None else None,
        ),
    ]
    specs: list[BakeoffBackendSpec] = []
    labels = ("a", "b")
    for index, backend in enumerate(names):
        model, manifest = pairs[index]
        label = labels[index]
        if model is None or manifest is None:
            raise BakeoffError(
                f"backend {backend!r} requires --model-{label} and --manifest-{label}"
            )
        specs.append(
            BakeoffBackendSpec(
                backend=backend,
                model=model,
                manifest=manifest,
                confidence=confidence,
                image_size=image_size,
                device=device,
                class_names=class_names,
            )
        )
    return tuple(specs)


def parse_backends_csv(value: str) -> tuple[str, ...]:
    """Split a comma-separated backends string; empty tokens fail closed."""
    if not isinstance(value, str) or not value.strip():
        raise BakeoffError("backends list must not be empty")
    parts = [part.strip() for part in value.split(",")]
    if not parts or any(not part for part in parts):
        raise BakeoffError("backends list must not be empty or contain empty names")
    return tuple(parts)


def _coerce_class_names(value: object) -> tuple[str, ...] | None:
    if value is None:
        return None
    if isinstance(value, (list, tuple)):
        names = tuple(str(item) for item in value)
        return names
    raise BakeoffError("class_names must be an array of strings when provided")
