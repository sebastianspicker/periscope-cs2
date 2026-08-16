from __future__ import annotations

import os
import subprocess
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.inference.providers import detect


def test_nvidia_smi_query_never_uses_ambient_path(tmp_path: Path) -> None:
    hostile = tmp_path / "nvidia-smi"
    hostile.write_text("#!/bin/sh\nexit 99\n", encoding="utf-8")
    hostile.chmod(0o755)

    with (
        patch.dict(os.environ, {"PATH": str(tmp_path)}, clear=False),
        patch.object(detect, "_PLATFORM_NVIDIA_SMI_PATHS", ()),
        patch.object(subprocess, "run") as run,
    ):
        assert detect._nvidia_smi_query() == {}

    run.assert_not_called()


def test_nvidia_smi_query_uses_validated_absolute_executable(tmp_path: Path) -> None:
    executable = tmp_path / "nvidia-smi"
    executable.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    executable.chmod(0o700)
    completed = subprocess.CompletedProcess(
        args=[],
        returncode=0,
        stdout="NVIDIA T4, 15360, 550.54\n",
        stderr="",
    )

    with (
        patch.dict(
            os.environ,
            {detect._TRUSTED_NVIDIA_SMI_ENV: str(executable)},
            clear=False,
        ),
        patch.object(subprocess, "run", return_value=completed) as run,
    ):
        assert detect._nvidia_smi_query() == {
            "name": "NVIDIA T4",
            "vram_mb": 15360,
            "driver": "550.54",
        }

    assert run.call_args.args[0] == [
        str(executable.resolve()),
        "--query-gpu=name,memory.total,driver_version",
        "--format=csv,noheader,nounits",
    ]
    assert run.call_args.kwargs["timeout"] == 5


def test_nvidia_smi_query_rejects_group_writable_configured_binary(tmp_path: Path) -> None:
    executable = tmp_path / "nvidia-smi"
    executable.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    executable.chmod(0o770)

    with (
        patch.dict(
            os.environ,
            {detect._TRUSTED_NVIDIA_SMI_ENV: str(executable)},
            clear=False,
        ),
        patch.object(subprocess, "run") as run,
    ):
        assert detect._nvidia_smi_query() == {}

    run.assert_not_called()
