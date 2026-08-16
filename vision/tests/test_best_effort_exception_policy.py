"""Recovery boundaries must not hide programming errors.

These tests use fakes so they do not require a display server, Gradio, torch,
or a GPU.  Each table pairs a documented recoverable backend/OS failure with
an unexpected exception that must still reach the caller.
"""

from __future__ import annotations

import importlib
import importlib.util
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, patch

from cs2_vision_access.capture import ScreenCaptureError
from cs2_vision_access.capture.overlay_backends.tkinter_backend import TkinterOverlayBackend
from cs2_vision_access.cli.handlers import live
from cs2_vision_access.training import train_core
from cs2_vision_access.training.auto import _note_events_path


class _Progress:
    def __call__(self, *args: object, **kwargs: object) -> None:
        del args, kwargs


def _load_space_app() -> types.ModuleType:
    """Load the Space app without importing optional Gradio dependencies."""
    gradio = types.ModuleType("gradio")
    gradio.Progress = _Progress  # type: ignore[attr-defined]
    app_path = Path(__file__).resolve().parents[1] / "src/cs2_vision_access/training/space/app.py"
    spec = importlib.util.spec_from_file_location("space_exception_policy_test", app_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load Space app test module")
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {"gradio": gradio}):
        spec.loader.exec_module(module)
    return module


_SPACE_APP = _load_space_app()


class _TclError(Exception):
    """Stand-in for the platform Tk error class."""


class _TkRoot:
    def __init__(self, *, bind_error: BaseException | None = None) -> None:
        self.bind_error = bind_error
        self.destroy_error: BaseException | None = None
        self.draw_error: BaseException | None = None
        self.poll_error: BaseException | None = None

    def title(self, title: str) -> None:
        del title

    def overrideredirect(self, enabled: bool) -> None:
        del enabled

    def attributes(self, *args: object) -> None:
        del args

    def geometry(self, geometry: str) -> None:
        del geometry

    def configure(self, **kwargs: object) -> None:
        del kwargs

    def bind(self, sequence: str, callback: object) -> None:
        del sequence, callback
        if self.bind_error is not None:
            raise self.bind_error

    def focus_force(self) -> None:
        return None

    def destroy(self) -> None:
        if self.destroy_error is not None:
            raise self.destroy_error

    def update_idletasks(self) -> None:
        if self.draw_error is not None:
            raise self.draw_error

    def update(self) -> None:
        if self.poll_error is not None:
            raise self.poll_error


class _TkCanvas:
    def __init__(self, *args: object, **kwargs: object) -> None:
        del args, kwargs

    def pack(self) -> None:
        return None

    def create_image(self, *args: object, **kwargs: object) -> int:
        del args, kwargs
        return 1

    def itemconfig(self, *args: object, **kwargs: object) -> None:
        del args, kwargs


def _fake_tk(root: _TkRoot) -> types.ModuleType:
    module = types.ModuleType("tkinter")
    module.TclError = _TclError
    module.Tk = MagicMock(return_value=root)
    module.Canvas = _TkCanvas
    module.PhotoImage = MagicMock()
    return module


class MonitorAndTrainingFallbackTests(unittest.TestCase):
    def test_monitor_discovery_recovers_only_backend_errors(self) -> None:
        cases: list[tuple[BaseException, bool]] = [
            (ScreenCaptureError("mss unavailable"), True),
            (OSError("display unavailable"), True),
            (KeyError("index"), False),
        ]
        for error, recovers in cases:
            with self.subTest(error=type(error).__name__):
                with patch.object(live, "list_monitors", side_effect=error):
                    if recovers:
                        self.assertEqual(live._monitor_origin(1), (0, 0))
                    else:
                        with self.assertRaises(type(error)):
                            live._monitor_origin(1)

    def test_training_device_probe_recovers_only_runtime_failures(self) -> None:
        cases: list[tuple[BaseException, bool]] = [
            (RuntimeError("CUDA driver unavailable"), True),
            (OSError("CUDA library unavailable"), True),
            (TypeError("bad torch API"), False),
        ]
        for error, recovers in cases:
            with self.subTest(error=type(error).__name__):
                torch = SimpleNamespace(
                    cuda=SimpleNamespace(is_available=MagicMock(side_effect=error))
                )
                with patch.dict(sys.modules, {"torch": torch}):
                    if recovers:
                        self.assertEqual(train_core.auto_train_device(), "cpu")
                    else:
                        with self.assertRaises(type(error)):
                            train_core.auto_train_device()

    def test_event_artifact_pointer_recovers_only_filesystem_errors(self) -> None:
        class ProbePath:
            def __init__(self, error: BaseException) -> None:
                self.error = error

            def __truediv__(self, name: str) -> ProbePath:
                del name
                return self

            def is_file(self) -> bool:
                raise self.error

        for error, recovers in [(OSError("read-only"), True), (KeyError("artifacts"), False)]:
            with self.subTest(error=type(error).__name__):
                paths = SimpleNamespace(run_dir=ProbePath(error))
                state = SimpleNamespace(artifacts={})
                if recovers:
                    _note_events_path(paths, state)
                else:
                    with self.assertRaises(type(error)):
                        _note_events_path(paths, state)


class TkinterFallbackTests(unittest.TestCase):
    def test_close_recovers_when_tkinter_is_unavailable(self) -> None:
        backend = TkinterOverlayBackend()
        backend._root = _TkRoot()
        backend._running = True

        with patch.dict(sys.modules, {"tkinter": None}):
            backend.close()

        self.assertIsNone(backend._root)
        self.assertFalse(backend._running)

    def test_tkinter_ui_boundaries_recover_only_tcl_errors(self) -> None:
        for operation in ("open", "close", "show_frame", "poll_events"):
            for error, recovers in [(_TclError("window closed"), True), (TypeError("bug"), False)]:
                with self.subTest(operation=operation, error=type(error).__name__):
                    root = _TkRoot()
                    backend = TkinterOverlayBackend()
                    if operation == "open":
                        root.bind_error = error
                    elif operation == "close":
                        backend._root = root
                        root.destroy_error = error
                    else:
                        backend._root = root
                        backend._canvas = _TkCanvas()
                        backend._running = True
                        backend._encode_photo = MagicMock(return_value=object())
                        if operation == "show_frame":
                            root.draw_error = error
                        else:
                            root.poll_error = error

                    with patch.dict(sys.modules, {"tkinter": _fake_tk(root)}):
                        if recovers:
                            if operation == "open":
                                backend.open(10, 10)
                            elif operation == "close":
                                backend.close()
                            elif operation == "show_frame":
                                backend.show_frame(object())
                            else:
                                self.assertFalse(backend.poll_events())
                        else:
                            with self.assertRaises(type(error)):
                                if operation == "open":
                                    backend.open(10, 10)
                                elif operation == "close":
                                    backend.close()
                                elif operation == "show_frame":
                                    backend.show_frame(object())
                                else:
                                    backend.poll_events()

    def test_hotkey_callback_errors_propagate(self) -> None:
        backend = TkinterOverlayBackend()
        backend.set_hotkey_handler(MagicMock(side_effect=TypeError("bug")))
        with self.assertRaises(TypeError):
            backend._on_key_press(SimpleNamespace(char="q", keysym="q"))


class SpaceFallbackTests(unittest.TestCase):
    def test_vendored_cloud_loader_recovers_only_import_or_os_errors(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            cloud_dir = Path(temporary_directory) / "cloud"
            cloud_dir.mkdir()
            (cloud_dir / "__init__.py").touch()
            for error, recovers in [
                (ImportError("dependency missing"), True),
                (TypeError("bug"), False),
            ]:
                with self.subTest(error=type(error).__name__):
                    with patch.object(importlib, "import_module", side_effect=error):
                        if recovers:
                            self.assertIsNone(_SPACE_APP._load_cloud_package_dir(cloud_dir))
                        else:
                            with self.assertRaises(type(error)):
                                _SPACE_APP._load_cloud_package_dir(cloud_dir)

    def test_cuda_probe_recovers_only_os_process_failures(self) -> None:
        executable = "/usr/local/bin/nvidia-smi"
        cases: list[tuple[BaseException, bool]] = [
            (OSError("not executable"), True),
            (subprocess.TimeoutExpired([executable], timeout=3), True),
            (TypeError("bad subprocess result"), False),
        ]
        for error, recovers in cases:
            with self.subTest(error=type(error).__name__):
                with (
                    patch.dict(sys.modules, {"torch": None}),
                    patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=executable),
                    patch.object(_SPACE_APP._gpu_security.subprocess, "run", side_effect=error),
                ):
                    if recovers:
                        self.assertFalse(_SPACE_APP._has_cuda())
                    else:
                        with self.assertRaises(type(error)):
                            _SPACE_APP._has_cuda()

    def test_gpu_info_recovers_only_known_torch_and_process_failures(self) -> None:
        torch_cases: list[tuple[BaseException, bool]] = [
            (RuntimeError("CUDA driver unavailable"), True),
            (TypeError("bad torch API"), False),
        ]
        for error, recovers in torch_cases:
            with self.subTest(source="torch", error=type(error).__name__):
                torch = SimpleNamespace(
                    cuda=SimpleNamespace(is_available=MagicMock(side_effect=error))
                )
                with (
                    patch.dict(sys.modules, {"torch": torch}),
                    patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=None),
                ):
                    if recovers:
                        self.assertEqual(_SPACE_APP._gpu_info(), "None (CPU)")
                    else:
                        with self.assertRaises(type(error)):
                            _SPACE_APP._gpu_info()

        executable = "/usr/local/bin/nvidia-smi"
        for error, recovers in [(OSError("not executable"), True), (TypeError("bug"), False)]:
            with self.subTest(source="nvidia-smi", error=type(error).__name__):
                with (
                    patch.dict(sys.modules, {"torch": None}),
                    patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=executable),
                    patch.object(_SPACE_APP._gpu_security.subprocess, "run", side_effect=error),
                ):
                    if recovers:
                        self.assertEqual(_SPACE_APP._gpu_info(), "None (CPU)")
                    else:
                        with self.assertRaises(type(error)):
                            _SPACE_APP._gpu_info()


if __name__ == "__main__":
    unittest.main()
