"""Regression coverage for production guards that must survive ``python -O``."""

from __future__ import annotations

import ast
import sys
import time
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock

import numpy as np
import pytest

from cs2_vision_access.capture.base import CaptureConfig
from cs2_vision_access.capture.hotkeys import LiveControlState
from cs2_vision_access.capture.manager import CaptureError, CaptureManager
from cs2_vision_access.capture.overlay_backends.win32 import Win32OverlayBackend
from cs2_vision_access.inference.pipeline.config import LivePipelineConfig
from cs2_vision_access.inference.pipeline.diagnostics import _Diagnostics
from cs2_vision_access.inference.pipeline.frame import run_frame
from cs2_vision_access.inference.providers import benchmark
from cs2_vision_access.labeling.review_io import _files_list
from cs2_vision_access.labeling.types import BootstrapError
from cs2_vision_access.renderer import OutlineRenderer, OutlineStyle
from cs2_vision_access.renderer.draw import OutlineRenderer as DrawOutlineRenderer
from cs2_vision_access.segmenters._preprocessing import box_to_sam_prompt


def test_production_assert_guards_are_explicit() -> None:
    """Invariant checks in the remediated production paths are never optimized away."""
    root = Path(__file__).resolve().parents[1]
    targets = (
        "scripts/remote_notebooks/__init__.py",
        "src/cs2_vision_access/capture/manager.py",
        "src/cs2_vision_access/capture/overlay_backends/win32.py",
        "src/cs2_vision_access/cli/handlers/bakeoff.py",
        "src/cs2_vision_access/inference/pipeline/frame.py",
        "src/cs2_vision_access/labeling/review_io.py",
        "src/cs2_vision_access/renderer/draw.py",
        "src/cs2_vision_access/training/prepare_lib/assets.py",
    )
    for target in targets:
        tree = ast.parse((root / target).read_text(encoding="utf-8"))
        assert not any(isinstance(node, ast.Assert) for node in ast.walk(tree)), target


def test_unopened_capture_info_fails_explicitly() -> None:
    manager = CaptureManager(CaptureConfig(), MagicMock())

    with pytest.raises(CaptureError, match="not opened"):
        _ = manager.info


def test_win32_lifecycle_requires_loaded_bindings() -> None:
    backend = Win32OverlayBackend()
    backend._window = 1

    with pytest.raises(RuntimeError, match="bindings are unavailable"):
        backend.move(20, 10)


def test_role_renderer_requires_catalog() -> None:
    renderer = DrawOutlineRenderer(catalog=None)

    with pytest.raises(RuntimeError, match="requires a treatment catalog"):
        renderer._render_with_catalog(
            MagicMock(), np.zeros((2, 2, 3), dtype=np.uint8), (), frame_index=0
        )


def test_successful_capture_without_frame_is_rejected() -> None:
    capture_mgr = MagicMock()
    capture_mgr.read.return_value = (True, None, "")

    with pytest.raises(RuntimeError, match="success without a frame"):
        run_frame(
            frame_index=0,
            capture_mgr=capture_mgr,
            segmenter=MagicMock(),
            cfg=LivePipelineConfig(headless=True, frame_skip_threshold=10_000),
            capture_fps=60.0,
            started=time.perf_counter(),
            base_outline_style=OutlineStyle(),
            renderer=OutlineRenderer(),
            temporal_policy=None,
            control_state=LiveControlState(),
            display_mgr=MagicMock(),
            on_frame=None,
            diag=_Diagnostics(),
            last_temporal_enabled=False,
            last_output_mode="overlay",
            last_alpha_fill=False,
        )


def test_draft_files_are_checked_as_objects() -> None:
    with pytest.raises(BootstrapError, match="list of objects"):
        _files_list({"files": ["not-a-draft-entry"]})


def test_box_prompt_shape_uses_numpy_supported_signature() -> None:
    coords, labels = box_to_sam_prompt((10.0, 20.0, 30.0, 40.0), (100, 200), 1024)

    assert coords.shape == (1, 2, 2)
    assert labels.shape == (1, 2)
    np.testing.assert_allclose(coords[0, 1], (153.6, 409.6))


def test_benchmark_only_skips_expected_backend_failures(monkeypatch: pytest.MonkeyPatch) -> None:
    def unavailable(*_args: object, **_kwargs: object) -> dict[str, object]:
        raise RuntimeError("provider unavailable")

    monkeypatch.setattr(benchmark, "benchmark_device", unavailable)
    assert benchmark._benchmark_candidate("model.onnx", "cuda:0") is None

    def programming_error(*_args: object, **_kwargs: object) -> dict[str, object]:
        raise KeyError("unexpected response")

    monkeypatch.setattr(benchmark, "benchmark_device", programming_error)
    with pytest.raises(KeyError, match="unexpected response"):
        benchmark._benchmark_candidate("model.onnx", "cuda:0")


def test_b112_handlers_have_explicit_exception_policy() -> None:
    root = Path(__file__).resolve().parents[1]
    targets = {
        "src/cs2_vision_access/inference/providers/benchmark.py": "_benchmark_candidate",
        "src/cs2_vision_access/gui/lifecycle.py": "_preview_photo",
        "src/cs2_vision_access/training/space/app.py": "_load_cloud_helpers",
    }
    for path, function_name in targets.items():
        tree = ast.parse((root / path).read_text(encoding="utf-8"))
        function = next(
            node
            for node in ast.walk(tree)
            if isinstance(node, ast.FunctionDef) and node.name == function_name
        )
        handlers = [node for node in ast.walk(function) if isinstance(node, ast.ExceptHandler)]
        assert handlers, function_name
        assert all(handler.type is not None for handler in handlers), function_name
        assert all(
            not isinstance(handler.type, ast.Name) or handler.type.id != "Exception"
            for handler in handlers
        ), function_name


def test_preview_only_swallows_tk_payload_errors() -> None:
    class PreviewError(Exception):
        pass

    app = SimpleNamespace(
        _tk=SimpleNamespace(
            TclError=PreviewError,
            PhotoImage=MagicMock(side_effect=PreviewError("bad png")),
        ),
        _log=MagicMock(),
    )
    from cs2_vision_access.gui.lifecycle import LifecycleMixin

    assert LifecycleMixin._preview_photo(app, b"not-a-png") is None
    app._log.assert_called_once_with("Preview frame skipped: bad png")


def test_remote_notebook_cell_guard_is_explicit(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    scripts = Path(__file__).resolve().parents[1] / "scripts"
    monkeypatch.syspath_prepend(str(scripts))
    sys.modules.pop("remote_notebooks", None)
    import remote_notebooks

    output = tmp_path / "src" / "cs2_vision_access" / "training" / "notebooks"
    output.mkdir(parents=True)
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(remote_notebooks, "build_colab_cells", lambda: [])

    with pytest.raises(RuntimeError, match="Colab expected"):
        remote_notebooks.main()
