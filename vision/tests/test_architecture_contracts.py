"""Architecture and public-interface contracts."""

from __future__ import annotations

import ast
import importlib
import pickle
import subprocess
import sys
from pathlib import Path

import pytest

from cs2_vision_access.application.configuration import LiveSettings
from cs2_vision_access.config import AppConfig, ConfigError, load_config, save_config
from cs2_vision_access.domain import InstanceMask, percentile


def test_new_public_layer_facades_import_without_runtime_side_effects() -> None:
    modules = (
        "cs2_vision_access.domain",
        "cs2_vision_access.application.ports",
        "cs2_vision_access.application.live",
        "cs2_vision_access.application.configuration",
        "cs2_vision_access.application.model_assets",
        "cs2_vision_access.workflows.dataset",
        "cs2_vision_access.workflows.labeling",
        "cs2_vision_access.workflows.training",
        "cs2_vision_access.workflows.evaluation",
        "cs2_vision_access.workflows.study",
        "cs2_vision_access.workflows.bakeoff",
        "cs2_vision_access.adapters.models",
        "cs2_vision_access.interfaces.cli",
        "cs2_vision_access.interfaces.gui",
    )
    for module in modules:
        assert importlib.import_module(module)
    assert LiveSettings is AppConfig
    assert percentile([1.0, 3.0], 50) == 2.0
    assert InstanceMask(0, ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0)), 1.0, 0, "person")


def test_inner_layers_do_not_reach_interfaces_or_adapters() -> None:
    package = Path(__file__).parents[1] / "src" / "cs2_vision_access"
    forbidden = {
        "domain": {"application", "workflows", "adapters", "interfaces"},
        "application": {"workflows", "adapters", "interfaces"},
        "workflows": {"adapters", "interfaces"},
        "adapters": {"workflows", "interfaces"},
    }
    prefix = "cs2_vision_access."
    for layer, banned_layers in forbidden.items():
        for source in (package / layer).rglob("*.py"):
            tree = ast.parse(source.read_text(encoding="utf-8"))
            imported = {
                node.module
                for node in ast.walk(tree)
                if isinstance(node, ast.ImportFrom) and node.module is not None
            }
            for module in imported:
                assert not any(
                    module.startswith(f"{prefix}{banned}") for banned in banned_layers
                ), f"{source} imports forbidden {module}"
            if layer != "interfaces":
                assert not any(
                    module.startswith((f"{prefix}cli", f"{prefix}gui")) for module in imported
                ), f"{source} imports legacy interface"


def test_config_v1_round_trip_and_path_policy(tmp_path: Path) -> None:
    destination = tmp_path / "config.json"
    config = AppConfig()
    assert save_config(config, destination) == destination
    assert load_config(destination) == config

    unsupported = tmp_path / "bad.json"
    unsupported.write_text('{"schema_version": 2}', encoding="utf-8")
    with pytest.raises(ConfigError, match="unsupported config schema_version"):
        load_config(unsupported)

    if hasattr(Path, "symlink_to"):
        link = tmp_path / "link.json"
        link.symlink_to(destination)
        with pytest.raises(ConfigError, match="destination must not be a symlink"):
            save_config(config, link, overwrite=True)


@pytest.mark.parametrize(
    "module",
    (
        "cs2_vision_access.training.contracts",
        "cs2_vision_access.training.remote_autonomous",
        "cs2_vision_access.evaluation.masks",
        "cs2_vision_access.evaluation.geometry",
        "cs2_vision_access.frames.extract",
    ),
)
def test_documented_legacy_submodule_facades_resolve(module: str) -> None:
    assert importlib.import_module(module)


def test_documented_legacy_submodule_facades_preserve_canonical_identity() -> None:
    legacy_config_package = importlib.import_module("cs2_vision_access.config")
    canonical_config_package = importlib.import_module(
        "cs2_vision_access.application.configuration"
    )
    legacy_config = importlib.import_module("cs2_vision_access.config.models")
    canonical_config = importlib.import_module("cs2_vision_access.application.configuration.models")
    legacy_dataset = importlib.import_module("cs2_vision_access.dataset.types")
    canonical_dataset = importlib.import_module("cs2_vision_access.workflows.dataset.types")
    legacy_masks = importlib.import_module("cs2_vision_access.evaluation.masks")
    canonical_masks = importlib.import_module("cs2_vision_access.workflows.evaluation.masks")
    legacy_geometry = importlib.import_module("cs2_vision_access.evaluation.geometry")
    canonical_geometry = importlib.import_module("cs2_vision_access.workflows.evaluation.geometry")
    legacy_frames = importlib.import_module("cs2_vision_access.frames.extract")
    canonical_frames = importlib.import_module("cs2_vision_access.workflows.dataset.frames.extract")
    legacy_segmenters = importlib.import_module("cs2_vision_access.segmenters.protocol")
    canonical_segmenters = importlib.import_module(
        "cs2_vision_access.adapters.models.segmenters.protocol"
    )
    legacy_contracts = importlib.import_module("cs2_vision_access.training.contracts")
    canonical_contracts = importlib.import_module("cs2_vision_access.workflows.training.contracts")

    assert legacy_config is canonical_config
    assert legacy_dataset is canonical_dataset
    assert legacy_masks is canonical_masks
    assert legacy_geometry is canonical_geometry
    assert legacy_frames is canonical_frames
    assert legacy_segmenters is canonical_segmenters
    assert legacy_contracts is canonical_contracts
    assert legacy_config_package.__path__ != canonical_config_package.__path__
    assert legacy_config.AppConfig is canonical_config.AppConfig
    assert legacy_config.ConfigError is canonical_config.ConfigError
    assert legacy_dataset.DatasetSummary is canonical_dataset.DatasetSummary
    assert legacy_masks.evaluate_predictions is canonical_masks.evaluate_predictions
    assert legacy_geometry.mask_iou is canonical_geometry.mask_iou
    assert legacy_frames.extract_frames is canonical_frames.extract_frames
    assert legacy_segmenters.Segmenter is canonical_segmenters.Segmenter
    assert legacy_contracts.Layout is canonical_contracts.Layout

    restored = pickle.loads(pickle.dumps(legacy_config.AppConfig()))
    assert type(restored) is canonical_config.AppConfig
    assert canonical_config.AppConfig.__module__ == canonical_config.__name__

    for former_private_module in (
        "cs2_vision_access.config.io",
        "cs2_vision_access.dataset.split",
        "cs2_vision_access.evaluation.temporal",
        "cs2_vision_access.training.local",
        "cs2_vision_access.training.remote_autonomous.bootstrap",
    ):
        with pytest.raises(ModuleNotFoundError):
            importlib.import_module(former_private_module)


def test_remote_autonomous_patch_target_is_the_phase_bindings_module() -> None:
    package = importlib.import_module("cs2_vision_access.workflows.training.remote_autonomous")
    public = importlib.import_module("cs2_vision_access.workflows.training.remote_autonomous.deps")
    bindings = importlib.import_module(
        "cs2_vision_access.workflows.training.remote_autonomous_bindings"
    )
    assert package.deps is public is bindings

    legacy_package = importlib.import_module("cs2_vision_access.training.remote_autonomous")
    legacy_deps = importlib.import_module("cs2_vision_access.training.remote_autonomous.deps")
    assert legacy_package.deps is legacy_deps is public


@pytest.mark.parametrize(
    "module",
    (
        "cs2_vision_access.training.prepare",
        "cs2_vision_access.training.train",
        "cs2_vision_access.training.bundle",
        "cs2_vision_access.training.auto",
    ),
)
def test_documented_training_module_help_is_forwarded(module: str) -> None:
    result = subprocess.run(
        [sys.executable, "-m", module, "--help"],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
