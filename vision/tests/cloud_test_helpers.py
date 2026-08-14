"""Shared helpers for cloud / training script tests."""

from __future__ import annotations

import zipfile
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[1]
_CLOUD_PKG = _REPO_ROOT / "src" / "cs2_vision_access" / "training" / "cloud"
_CLOUD_INIT = _CLOUD_PKG / "__init__.py"
_SPACE_APP_PY = _REPO_ROOT / "src" / "cs2_vision_access" / "training" / "space" / "app.py"


def _write_minimal_zip(
    zip_path: Path,
    *,
    nested_root: str | None = None,
    images: list[str] | None = None,
    with_labels: bool = True,
    with_yaml: bool = False,
) -> None:
    """Build a tiny YOLO-style dataset zip for extract tests."""
    images = images if images is not None else ["frame_0001.jpg"]
    prefix = f"{nested_root}/" if nested_root else ""
    with zipfile.ZipFile(zip_path, "w") as zf:
        for name in images:
            zf.writestr(f"{prefix}images/{name}", b"fake-image-bytes")
            if with_labels:
                stem = Path(name).stem
                zf.writestr(f"{prefix}labels/{stem}.txt", "0 0.5 0.5 0.1 0.1\n")
        if with_yaml:
            zf.writestr(
                f"{prefix}dataset.yaml",
                "path: .\ntrain: images\nval: images\nnc: 4\nnames:\n  0: ct\n",
            )
