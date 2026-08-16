"""Dependency installation for cloud notebook environments."""

from __future__ import annotations

import sys
from pathlib import Path
from subprocess import CalledProcessError, TimeoutExpired, run  # nosec B404

_ALLOWED_PIP_PACKAGES = frozenset(
    {"ultralytics", "onnx", "onnxconverter-common", "onnxruntime-gpu"}
)
_PIP_INSTALL_TIMEOUT_SECONDS = 300


def _trusted_python_executable() -> str:
    """Return the absolute interpreter path used for the fixed pip invocation."""
    try:
        executable = Path(sys.executable).resolve(strict=True)
    except (OSError, TypeError) as exc:
        raise RuntimeError("cannot resolve the active Python interpreter") from exc
    if not executable.is_absolute() or not executable.is_file():
        raise RuntimeError("active Python interpreter is not a trusted executable file")
    return str(executable)


def _install_allowed_packages(packages: list[str]) -> None:
    """Install the fixed dependency allowlist without a shell or ambient binary."""
    unexpected = set(packages) - _ALLOWED_PIP_PACKAGES
    if unexpected:
        raise RuntimeError(f"refusing to install unapproved packages: {sorted(unexpected)}")
    command = [_trusted_python_executable(), "-m", "pip", "install", "-q", *packages]
    try:
        # `command` is assembled only from the validated interpreter and package allowlist above.
        run(  # nosec B603  # nosemgrep: dangerous-subprocess-use-audit
            command,
            check=True,
            shell=False,
            timeout=_PIP_INSTALL_TIMEOUT_SECONDS,
        )
    except CalledProcessError as exc:
        raise RuntimeError(
            f"dependency installation failed with exit code {exc.returncode}"
        ) from exc
    except TimeoutExpired as exc:
        raise RuntimeError(
            f"dependency installation timed out after {_PIP_INSTALL_TIMEOUT_SECONDS} seconds"
        ) from exc
    except OSError as exc:
        raise RuntimeError(f"could not start dependency installation: {exc}") from exc


def install_dependencies(gpu: bool = True) -> None:
    """Install Ultralytics + ONNX packages when missing.

    Always attempts ``ultralytics``, ``onnx``, and ``onnxconverter-common`` if
    any required import is missing. Optionally installs ``onnxruntime-gpu``
    when ``gpu=True``.

    Prints platform-oriented hints for Colab (GPU free tier) vs CPU-only runs.
    """
    missing: list[str] = []

    try:
        import ultralytics  # noqa: F401
    except ImportError:
        missing.append("ultralytics")

    try:
        import onnx  # type: ignore[import-not-found]  # noqa: F401
    except ImportError:
        missing.append("onnx")

    try:
        import onnxconverter_common  # type: ignore[import-not-found]  # noqa: F401
    except ImportError:
        missing.append("onnxconverter-common")

    packages = list(missing)
    if gpu:
        try:
            import onnxruntime  # type: ignore[import-untyped]  # noqa: F401

            # If ORT is present but may be CPU-only; still try GPU package when requested.
            # Skip reinstall only when we already have a working CUDA EP (best-effort).
            providers: list[str] = []
            try:
                providers = list(onnxruntime.get_available_providers())
            except Exception:
                providers = []
            if "CUDAExecutionProvider" not in providers:
                packages.append("onnxruntime-gpu")
        except ImportError:
            packages.append("onnxruntime-gpu")

    if packages:
        print(f"Installing dependencies: {', '.join(packages)}...")
        _install_allowed_packages(packages)
        print("✓ dependencies installed")
    else:
        print("✓ ultralytics / onnx / onnxconverter-common already installed")

    try:
        import torch

        cuda_ok = torch.cuda.is_available()
        print(f"CUDA available: {cuda_ok}")
        if cuda_ok:
            print(f"GPU: {torch.cuda.get_device_name(0)}")
            print(
                "  Tip: Colab free tier GPU is fine for YOLO11n-seg; use FP16 ONNX for inference."
            )
        else:
            print(
                "  Tip: running on CPU — training will be slow. "
                "On Colab: Runtime → Change runtime type → GPU. "
                "On Kaggle: enable GPU accelerator in notebook settings."
            )
    except ImportError:
        print("  Warning: torch not importable after install; training may fail.")
