"""Checksum manifests for local ONNX runtime models."""

from __future__ import annotations

import hashlib
import hmac
import json
import os
import re
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
ALLOWED_TASK = "instance-segmentation"
MAX_MODEL_BYTES = 2_000_000_000
_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
_MANIFEST_KEYS = frozenset(
    {
        "schema_version",
        "model_filename",
        "sha256",
        "task",
        "classes",
        "origin",
        "license",
    }
)


class ModelManifestError(ValueError):
    """A model or manifest failed a deterministic trust check."""


@dataclass(frozen=True)
class ModelManifest:
    schema_version: int
    model_filename: str
    sha256: str
    task: str
    classes: dict[int, str]
    origin: str
    license: str

    @classmethod
    def load(cls, path: str | Path) -> ModelManifest:
        manifest_path = _regular_file(path, suffix=".json", label="manifest")
        try:
            raw = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ModelManifestError(f"could not read manifest: {error}") from error
        if not isinstance(raw, dict):
            raise ModelManifestError("manifest root must be a JSON object")
        keys = frozenset(raw)
        if keys != _MANIFEST_KEYS:
            missing = sorted(_MANIFEST_KEYS - keys)
            unknown = sorted(keys - _MANIFEST_KEYS)
            raise ModelManifestError(
                f"manifest keys do not match schema; missing={missing}, unknown={unknown}"
            )
        return cls._from_mapping(raw)

    @classmethod
    def _from_mapping(cls, raw: dict[str, Any]) -> ModelManifest:
        version = raw["schema_version"]
        if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
            raise ModelManifestError(f"schema_version must be {SCHEMA_VERSION}")

        model_filename = _required_text(raw["model_filename"], "model_filename")
        if Path(model_filename).name != model_filename or not model_filename.endswith(".onnx"):
            raise ModelManifestError("model_filename must be a basename ending in .onnx")

        sha256 = _required_text(raw["sha256"], "sha256").lower()
        if not _SHA256_PATTERN.fullmatch(sha256):
            raise ModelManifestError("sha256 must contain exactly 64 hexadecimal characters")

        task = _required_text(raw["task"], "task")
        if task != ALLOWED_TASK:
            raise ModelManifestError(f"task must be {ALLOWED_TASK!r}")

        classes = _parse_classes(raw["classes"])
        return cls(
            schema_version=version,
            model_filename=model_filename,
            sha256=sha256,
            task=task,
            classes=classes,
            origin=_required_text(raw["origin"], "origin"),
            license=_required_text(raw["license"], "license"),
        )

    def as_json(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "model_filename": self.model_filename,
            "sha256": self.sha256,
            "task": self.task,
            "classes": {str(key): value for key, value in sorted(self.classes.items())},
            "origin": self.origin,
            "license": self.license,
        }


def verify_model(
    model_path: str | Path,
    manifest_path: str | Path,
    *,
    max_model_bytes: int = MAX_MODEL_BYTES,
) -> tuple[Path, ModelManifest]:
    """Verify path, size, filename, format, and SHA-256 before model loading."""
    if max_model_bytes <= 0:
        raise ValueError("max_model_bytes must be positive")
    model = _regular_file(model_path, suffix=".onnx", label="model")
    if model.stat().st_size > max_model_bytes:
        raise ModelManifestError(f"model exceeds the {max_model_bytes}-byte runtime safety limit")
    manifest = ModelManifest.load(manifest_path)
    if model.name != manifest.model_filename:
        raise ModelManifestError(
            f"model filename {model.name!r} does not match manifest {manifest.model_filename!r}"
        )
    actual_digest = sha256_file(model)
    if not hmac.compare_digest(actual_digest, manifest.sha256):
        raise ModelManifestError(
            f"model SHA-256 mismatch: expected {manifest.sha256}, got {actual_digest}"
        )
    return model, manifest


def create_manifest(
    model_path: str | Path,
    manifest_path: str | Path,
    *,
    classes: dict[int, str],
    origin: str,
    license_name: str,
    overwrite: bool = False,
) -> Path:
    """Create an adjacent-style manifest after an explicit trust decision."""
    model = _regular_file(model_path, suffix=".onnx", label="model")
    parsed_classes = _parse_classes({str(key): value for key, value in classes.items()})
    manifest = ModelManifest(
        schema_version=SCHEMA_VERSION,
        model_filename=model.name,
        sha256=sha256_file(model),
        task=ALLOWED_TASK,
        classes=parsed_classes,
        origin=_required_text(origin, "origin"),
        license=_required_text(license_name, "license"),
    )

    destination = Path(manifest_path)
    if destination.resolve() == model:
        raise ModelManifestError("manifest destination must differ from the ONNX model")
    if destination.suffix.lower() != ".json":
        raise ModelManifestError("manifest destination must use the .json extension")
    if destination.is_symlink():
        raise ModelManifestError("manifest destination must not be a symlink")
    if destination.exists() and not overwrite:
        raise ModelManifestError(
            f"manifest already exists: {destination}; pass --overwrite to replace it"
        )
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.parent.is_symlink():
        raise ModelManifestError("manifest parent must not be a symlink")

    payload = json.dumps(manifest.as_json(), indent=2, sort_keys=True) + "\n"
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
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, destination)
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return destination


def sha256_file(path: str | Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _regular_file(path: str | Path, *, suffix: str, label: str) -> Path:
    candidate = Path(path)
    if candidate.is_symlink():
        raise ModelManifestError(f"{label} path must not be a symlink")
    if not candidate.is_file():
        raise ModelManifestError(f"{label} is not a regular file: {candidate}")
    if candidate.suffix.lower() != suffix:
        raise ModelManifestError(f"{label} must use the {suffix} format")
    return candidate.resolve()


def _required_text(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ModelManifestError(f"{field} must be a non-empty string")
    if "\x00" in value:
        raise ModelManifestError(f"{field} must not contain NUL")
    return value.strip()


def _parse_classes(raw: object) -> dict[int, str]:
    if not isinstance(raw, dict) or not raw:
        raise ModelManifestError("classes must be a non-empty JSON object")
    parsed: dict[int, str] = {}
    for key, value in raw.items():
        if not isinstance(key, str) or not key.isascii() or not key.isdecimal():
            raise ModelManifestError("class keys must be non-negative decimal strings")
        class_id = int(key)
        parsed[class_id] = _required_text(value, f"classes[{key}]")
    expected = set(range(len(parsed)))
    if set(parsed) != expected:
        raise ModelManifestError("class ids must be contiguous and start at zero")
    folded = [name.casefold() for name in parsed.values()]
    if len(folded) != len(set(folded)):
        raise ModelManifestError("class names must be unique ignoring case")
    return dict(sorted(parsed.items()))
