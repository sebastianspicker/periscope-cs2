"""Platform-specific install cell source for Colab / Kaggle notebooks."""

from __future__ import annotations

_INSTALL_COMMON_HEAD = r"""# @title 1) Install + import (autonomous)
import os, sys, subprocess
from pathlib import Path

def _pip(*pkgs):
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-q", *pkgs])

# Core training + export deps
_pip(
    "ultralytics",
    "onnx",
    "onnxconverter-common",
    "opencv-python-headless",
    "huggingface_hub",
    "matplotlib",
    "pyyaml",
)
try:
    import torch  # noqa: F401
except Exception:
    _pip("torch")
try:
    _pip("onnxruntime-gpu")
except Exception as e:
    print("onnxruntime-gpu optional:", e)
"""

_INSTALL_IMPORT_AND_CUDA = r"""
# Fail fast if the autonomous entry point is still missing
try:
    from cs2_vision_access.training.remote_autonomous import (
        run_autonomous_loop,
        resolve_remote_dataset_zip,
    )
except Exception as e:
    raise SystemExit(
        "run_autonomous_loop still not importable after install/clone.\n"
        f"  last error: {e}\n"
        "  Set CS2_VISION_REPO=git+https://github.com/<you>/computer-vision-accessibility.git\n"
        "  or clone the repo into the working directory and re-run this cell."
    ) from e

import torch
print("remote_autonomous ready")
print("CUDA:", torch.cuda.is_available(), end=" ")
if torch.cuda.is_available():
    print(torch.cuda.get_device_name(0))
else:
    print(
        "\nWarning: no GPU detected; training will be slow. "
        "For Colab: Runtime → Change runtime type → T4 GPU. "
        "Smoke runs on CPU are allowed."
    )
"""

_INSTALL_RESOLVE_PACKAGE = r"""
def _try_import() -> bool:
    try:
        import cs2_vision_access.training.remote_autonomous  # noqa: F401
        return True
    except Exception:
        return False

# 1) Optional: pip install from CS2_VISION_REPO (git+https://... or path)
REPO = os.environ.get("CS2_VISION_REPO", "").strip()
if REPO:
    print("Installing package from CS2_VISION_REPO:", REPO)
    try:
        _pip(REPO)
    except Exception as e:
        print("CS2_VISION_REPO install failed:", e)

# 2) Editable / sys.path local clone under platform roots
"""


def _colab_install_cell() -> str:
    return (
        _INSTALL_COMMON_HEAD
        + _INSTALL_RESOLVE_PACKAGE
        + r"""
_CLONE_ROOTS = [Path("/content"), Path(".")]
_LOCAL_CANDIDATES = [
    Path("/content/computer-vision-accessibility"),
    Path("computer-vision-accessibility"),
    Path("/content"),
    Path("."),
]
for cand in _LOCAL_CANDIDATES:
    if (cand / "pyproject.toml").is_file() and (cand / "src" / "cs2_vision_access").is_dir():
        print("Editable/local package:", cand)
        try:
            _pip("-e", str(cand))
        except Exception:
            sys.path.insert(0, str(cand / "src"))
        break

for root in _CLONE_ROOTS:
    src = root / "src"
    if (src / "cs2_vision_access").is_dir():
        p = str(src)
        if p not in sys.path:
            sys.path.insert(0, p)

# 3) If still not importable: shallow git clone + pip -e / sys.path
if not _try_import():
    git_url = os.environ.get("CS2_VISION_GIT", "").strip()
    if not git_url:
        # Placeholder public URL; override with CS2_VISION_GIT if your fork differs
        git_url = "https://github.com/xai-org/computer-vision-accessibility.git"
    dest = Path("/content/computer-vision-accessibility")
    if not (dest / "src" / "cs2_vision_access").is_dir():
        print("Cloning package (depth=1):", git_url, "→", dest)
        try:
            subprocess.check_call(
                ["git", "clone", "--depth", "1", git_url, str(dest)],
            )
        except Exception as e:
            raise SystemExit(
                f"git clone failed ({e}).\n"
                "Set CS2_VISION_REPO=git+https://github.com/<you>/computer-vision-accessibility.git\n"
                "or clone repo into /content and re-run"
            ) from e
    try:
        _pip("-e", str(dest))
    except Exception:
        sys.path.insert(0, str(dest / "src"))
"""
        + _INSTALL_IMPORT_AND_CUDA
    )


def _kaggle_install_cell() -> str:
    return (
        _INSTALL_COMMON_HEAD
        + _INSTALL_RESOLVE_PACKAGE
        + r"""
_CLONE_ROOTS = [Path("/kaggle/working"), Path("/kaggle/input"), Path(".")]
_LOCAL_CANDIDATES = [
    Path("/kaggle/working/computer-vision-accessibility"),
    Path("/kaggle/input/computer-vision-accessibility"),
    Path("computer-vision-accessibility"),
    Path("/kaggle/working"),
    Path("."),
]
for cand in _LOCAL_CANDIDATES:
    if (cand / "pyproject.toml").is_file() and (cand / "src" / "cs2_vision_access").is_dir():
        print("Editable/local package:", cand)
        try:
            _pip("-e", str(cand))
        except Exception:
            sys.path.insert(0, str(cand / "src"))
        break

for root in _CLONE_ROOTS:
    src = root / "src"
    if (src / "cs2_vision_access").is_dir():
        p = str(src)
        if p not in sys.path:
            sys.path.insert(0, p)
    pkg = root / "computer-vision-accessibility"
    if (pkg / "pyproject.toml").is_file() and (pkg / "src" / "cs2_vision_access").is_dir():
        try:
            _pip("-e", str(pkg))
        except Exception:
            p = str(pkg / "src")
            if p not in sys.path:
                sys.path.insert(0, p)

# 3) If still not importable: shallow git clone + pip -e / sys.path
if not _try_import():
    git_url = os.environ.get("CS2_VISION_GIT", "").strip()
    if not git_url:
        git_url = "https://github.com/xai-org/computer-vision-accessibility.git"
    dest = Path("/kaggle/working/computer-vision-accessibility")
    if not (dest / "src" / "cs2_vision_access").is_dir():
        print("Cloning package (depth=1):", git_url, "→", dest)
        try:
            subprocess.check_call(
                ["git", "clone", "--depth", "1", git_url, str(dest)],
            )
        except Exception as e:
            raise SystemExit(
                f"git clone failed ({e}).\n"
                "Set CS2_VISION_REPO=git+https://github.com/<you>/computer-vision-accessibility.git\n"
                "or clone repo into /kaggle/working and re-run"
            ) from e
    try:
        _pip("-e", str(dest))
    except Exception:
        sys.path.insert(0, str(dest / "src"))
"""
        + _INSTALL_IMPORT_AND_CUDA
    )
