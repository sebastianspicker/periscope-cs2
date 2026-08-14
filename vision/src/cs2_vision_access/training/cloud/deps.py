"""Dependency installation for cloud notebook environments."""

from __future__ import annotations

import subprocess
import sys


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
        import onnx  # noqa: F401
    except ImportError:
        missing.append("onnx")

    try:
        import onnxconverter_common  # noqa: F401
    except ImportError:
        missing.append("onnxconverter-common")

    packages = list(missing)
    if gpu:
        try:
            import onnxruntime  # noqa: F401

            # If ORT is present but may be CPU-only; still try GPU package when requested.
            # Skip reinstall only when we already have a working CUDA EP (best-effort).
            providers: list[str] = []
            try:
                providers = list(onnxruntime.get_available_providers())  # type: ignore[attr-defined]
            except Exception:
                providers = []
            if "CUDAExecutionProvider" not in providers:
                packages.append("onnxruntime-gpu")
        except ImportError:
            packages.append("onnxruntime-gpu")

    if packages:
        print(f"Installing dependencies: {', '.join(packages)}...")
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "-q", *packages],
            check=True,
        )
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
