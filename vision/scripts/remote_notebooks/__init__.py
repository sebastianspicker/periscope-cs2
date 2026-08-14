"""Generate autonomous Colab/Kaggle notebooks (package entry)."""

from __future__ import annotations

from pathlib import Path

from .colab import build_colab_cells
from .format import write_nb
from .kaggle import build_kaggle_cells


def main() -> None:
    base = Path("src/cs2_vision_access/training/notebooks")
    colab = build_colab_cells()
    kaggle = build_kaggle_cells()
    write_nb(base / "colab.ipynb", colab)
    write_nb(base / "kaggle.ipynb", kaggle)
    assert len(colab) >= 7, f"Colab expected ≥7 cells, got {len(colab)}"
    assert len(kaggle) >= 7, f"Kaggle expected ≥7 cells, got {len(kaggle)}"


__all__ = [
    "main",
    "build_colab_cells",
    "build_kaggle_cells",
]
