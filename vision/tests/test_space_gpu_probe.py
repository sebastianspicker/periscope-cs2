"""Tests for the Hugging Face Space's shell-free GPU probe fallback."""

from __future__ import annotations

import importlib.util
import subprocess
import sys
import unittest
from pathlib import Path
from types import ModuleType, SimpleNamespace
from unittest.mock import patch


class _Progress:
    def __call__(self, *args: object, **kwargs: object) -> None:
        return None


def _load_space_app() -> ModuleType:
    """Load the app without requiring Gradio in the CPU test environment."""
    gradio = ModuleType("gradio")
    gradio.Progress = _Progress  # type: ignore[attr-defined]
    app_path = (
        Path(__file__).resolve().parents[1]
        / "src/cs2_vision_access/training/space/app.py"
    )
    spec = importlib.util.spec_from_file_location("space_app_gpu_probe_test", app_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load Space app test module")
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {"gradio": gradio}):
        spec.loader.exec_module(module)
    return module


_SPACE_APP = _load_space_app()


class SpaceGpuProbeTests(unittest.TestCase):
    @staticmethod
    def _without_torch() -> object:
        return patch.dict(sys.modules, {"torch": None})

    def test_has_cuda_treats_absent_binary_as_cpu_without_subprocess(self) -> None:
        with (
            self._without_torch(),
            patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=None),
            patch.object(_SPACE_APP._gpu_security.subprocess, "run") as run,
        ):
            self.assertFalse(_SPACE_APP._has_cuda())
        run.assert_not_called()

    def test_has_cuda_treats_untrusted_binary_as_cpu_without_subprocess(self) -> None:
        with (
            self._without_torch(),
            patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=None),
            patch.object(_SPACE_APP._gpu_security.subprocess, "run") as run,
        ):
            self.assertFalse(_SPACE_APP._has_cuda())
        run.assert_not_called()

    def test_has_cuda_uses_absolute_resolved_binary(self) -> None:
        executable = "/usr/local/bin/nvidia-smi"
        with (
            self._without_torch(),
            patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=executable),
            patch.object(
                _SPACE_APP._gpu_security.subprocess,
                "run",
                return_value=SimpleNamespace(returncode=0),
            ) as run,
        ):
            self.assertTrue(_SPACE_APP._has_cuda())
        run.assert_called_once_with([executable], capture_output=True, check=False, timeout=3)

    def test_gpu_info_uses_absolute_binary_and_literal_flags(self) -> None:
        executable = "/usr/local/bin/nvidia-smi"
        with (
            self._without_torch(),
            patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=executable),
            patch.object(
                _SPACE_APP._gpu_security.subprocess,
                "run",
                return_value=SimpleNamespace(returncode=0, stdout="NVIDIA T4, 16384 MiB\n"),
            ) as run,
        ):
            self.assertEqual(_SPACE_APP._gpu_info(), "NVIDIA T4")
        run.assert_called_once_with(
            [executable, "--query-gpu=name,memory.total", "--format=csv,noheader"],
            capture_output=True,
            text=True,
            check=False,
            timeout=3,
        )

    def test_gpu_info_falls_back_for_nonzero_exit_and_timeout(self) -> None:
        executable = "/usr/local/bin/nvidia-smi"
        for side_effect in (
            None,
            subprocess.TimeoutExpired([executable], timeout=3),
        ):
            with self.subTest(side_effect=side_effect):
                result = SimpleNamespace(returncode=1, stdout="")
                with (
                    self._without_torch(),
                    patch.object(_SPACE_APP, "_nvidia_smi_path", return_value=executable),
                    patch.object(
                        _SPACE_APP._gpu_security.subprocess,
                        "run",
                        return_value=result,
                        side_effect=side_effect,
                    ),
                ):
                    self.assertEqual(_SPACE_APP._gpu_info(), "None (CPU)")


if __name__ == "__main__":
    unittest.main()
