"""Load and validate auto-train configuration."""

from __future__ import annotations

import json
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from .parse_sections import (
    _apply_self_train_autonomous_defaults,
    _parse_eval,
    _parse_export,
    _parse_human_gate,
    _parse_label,
    _parse_paths,
    _parse_resume,
    _parse_self_train,
    _parse_sources,
    _parse_train,
)
from .parse_utils import _require_mapping
from .types import (
    SCHEMA_VERSION,
    SUPPORTED_MODES,
    AutoTrainConfig,
    AutoTrainConfigError,
    SourcesConfig,
    TrainConfig,
)


def _validate_mode_sources_backend(
    mode: str,
    sources: SourcesConfig,
    train: TrainConfig,
) -> None:
    """Cross-field consistency checks after parsing sub-objects."""
    if mode == "flat_cloud" and (
        sources.dataset_zip is None and sources.prebuilt_flat_root is None
    ):
        raise AutoTrainConfigError(
            "flat_cloud requires sources.dataset_zip or sources.prebuilt_flat_root"
        )
    if mode == "session_split" and (
        sources.prebuilt_dataset_root is None
        and sources.staging_root is None
        and not sources.videos
    ):
        raise AutoTrainConfigError(
            "session_split requires sources.prebuilt_dataset_root, "
            "sources.staging_root, or sources.videos"
        )


def config_from_mapping(payload: Mapping[str, Any]) -> AutoTrainConfig:
    """Build :class:`AutoTrainConfig` from a decoded mapping."""
    if not isinstance(payload, Mapping):
        raise AutoTrainConfigError("config root must be an object")
    version = payload.get("schema_version", SCHEMA_VERSION)
    if isinstance(version, bool) or not isinstance(version, int) or version != SCHEMA_VERSION:
        raise AutoTrainConfigError(f"schema_version must be {SCHEMA_VERSION}")
    mode = payload.get("mode")
    if not isinstance(mode, str) or mode not in SUPPORTED_MODES:
        raise AutoTrainConfigError(f"mode must be one of {sorted(SUPPORTED_MODES)}")
    run_id = payload.get("run_id")
    if not isinstance(run_id, str) or not run_id.strip():
        raise AutoTrainConfigError("run_id must be a non-empty string")
    sources = _parse_sources(_require_mapping(payload.get("sources"), "sources"))
    train = _parse_train(_require_mapping(payload.get("train"), "train"))
    _validate_mode_sources_backend(mode, sources, train)

    # Root-level autonomous convenience: unattended multi-iter defaults.
    # Does NOT force label.teacher=edgesam (needs local ONNX assets).
    root_autonomous = bool(payload.get("autonomous", False))
    st_raw = dict(_require_mapping(payload.get("self_train"), "self_train"))
    label_raw = dict(_require_mapping(payload.get("label"), "label"))
    human_gate_raw = dict(_require_mapping(payload.get("human_gate"), "human_gate"))

    if root_autonomous:
        st_raw = _apply_self_train_autonomous_defaults(st_raw, default_iterations=3)
        st_raw["autonomous"] = True
        label_raw["enabled"] = True
        if "bootstrap" not in label_raw:
            label_raw["bootstrap"] = True
        # Do not force teacher=edgesam — operators set it when models are ready.
        # Default gate off for unattended runs. Explicit block=true is rejected
        # by the validation below (do not silently swallow it).
        if not bool(human_gate_raw.get("block", False)):
            human_gate_raw["enabled"] = False
            human_gate_raw["block"] = False

    self_train = _parse_self_train(st_raw)
    human_gate = _parse_human_gate(human_gate_raw)

    # Unattended multi-iter must not be blocked by human_gate.block.
    if human_gate.block and (self_train.iterations > 1 or self_train.autonomous or root_autonomous):
        raise AutoTrainConfigError(
            "human_gate.block=true is incompatible with multi-iter self_train "
            "(iterations > 1) or autonomous=true; disable the gate for unattended runs"
        )

    return AutoTrainConfig(
        schema_version=version,
        mode=mode,
        run_id=run_id.strip(),
        paths=_parse_paths(_require_mapping(payload.get("paths"), "paths")),
        sources=sources,
        train=train,
        export=_parse_export(_require_mapping(payload.get("export"), "export")),
        label=_parse_label(label_raw),
        eval=_parse_eval(_require_mapping(payload.get("eval"), "eval")),
        self_train=self_train,
        resume=_parse_resume(_require_mapping(payload.get("resume"), "resume")),
        human_gate=human_gate,
    )


def load_config(path: str | Path) -> AutoTrainConfig:
    """Load auto-train config from a local ``.json`` / ``.yaml`` / ``.yml`` file."""
    candidate = Path(path)
    if candidate.is_symlink():
        raise AutoTrainConfigError("config path must not be a symlink")
    if not candidate.is_file():
        raise AutoTrainConfigError(f"config is not a regular file: {candidate}")
    suffix = candidate.suffix.lower()
    try:
        text = candidate.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        raise AutoTrainConfigError(f"could not read config: {error}") from error

    if suffix == ".json":
        try:
            payload = json.loads(text)
        except json.JSONDecodeError as error:
            raise AutoTrainConfigError(f"invalid JSON config: {error}") from error
    elif suffix in {".yaml", ".yml"}:
        try:
            import yaml
        except ImportError as error:  # pragma: no cover - dependency boundary
            raise AutoTrainConfigError(
                "PyYAML is required to load YAML auto-train configs"
            ) from error
        try:
            payload = yaml.safe_load(text)
        except yaml.YAMLError as error:
            raise AutoTrainConfigError(f"invalid YAML config: {error}") from error
    else:
        raise AutoTrainConfigError("config must use .json, .yaml, or .yml extension")

    if not isinstance(payload, dict):
        raise AutoTrainConfigError("config root must be an object")
    return config_from_mapping(payload)
