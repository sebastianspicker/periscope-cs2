"""Tests for scripts/_gen_remote_notebooks.py and scripts/remote_notebooks/."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = REPO_ROOT / "scripts"


def _ensure_scripts_on_path() -> None:
    s = str(SCRIPTS)
    if s not in sys.path:
        sys.path.insert(0, s)


@pytest.fixture(scope="module")
def rn():
    """Import the real remote_notebooks package shipped under scripts/."""
    _ensure_scripts_on_path()
    import remote_notebooks  # type: ignore[import-not-found]
    import remote_notebooks.format as format_mod  # type: ignore[import-not-found]
    import remote_notebooks.install as install_mod  # type: ignore[import-not-found]
    import remote_notebooks.shared as shared_mod  # type: ignore[import-not-found]

    return {
        "pkg": remote_notebooks,
        "format": format_mod,
        "install": install_mod,
        "shared": shared_mod,
    }


def _cell_text(cell: dict) -> str:
    src = cell.get("source", [])
    if isinstance(src, list):
        return "".join(src)
    return str(src)


def test_format_md_and_code_shapes(rn) -> None:
    fmt = rn["format"]
    m = fmt.md("# title")
    assert m["cell_type"] == "markdown"
    assert m["source"] == ["# title\n"]
    c = fmt.code("print(1)")
    assert c["cell_type"] == "code"
    assert c["outputs"] == []
    assert c["execution_count"] is None
    assert c["source"] == ["print(1)\n"]


def test_write_nb_roundtrip(rn, tmp_path: Path) -> None:
    fmt = rn["format"]
    path = tmp_path / "sample.ipynb"
    cells = [fmt.md("hi"), fmt.code("x = 1")]
    fmt.write_nb(path, cells)
    doc = json.loads(path.read_text(encoding="utf-8"))
    assert doc["nbformat"] == 4
    assert doc["nbformat_minor"] == 5
    assert len(doc["cells"]) == 2
    assert doc["cells"][0]["cell_type"] == "markdown"
    assert doc["cells"][1]["cell_type"] == "code"
    assert "kernelspec" in doc["metadata"]


def test_build_colab_cells_structure_and_markers(rn) -> None:
    cells = rn["pkg"].build_colab_cells()
    assert len(cells) >= 7
    types = [c["cell_type"] for c in cells]
    assert "markdown" in types
    assert "code" in types
    blob = "\n".join(_cell_text(c) for c in cells)
    assert "run_autonomous_loop" in blob
    assert "USE_CS2_10K" in blob
    assert "/content" in blob
    assert "cloud_t4" in blob
    # Colab-specific install roots
    assert 'Path("/content")' in blob or 'Path("/content")' in blob


def test_build_kaggle_cells_structure_and_markers(rn) -> None:
    cells = rn["pkg"].build_kaggle_cells()
    assert len(cells) >= 7
    types = [c["cell_type"] for c in cells]
    assert "markdown" in types
    assert "code" in types
    blob = "\n".join(_cell_text(c) for c in cells)
    assert "run_autonomous_loop" in blob
    assert "USE_CS2_10K" in blob
    assert "/kaggle" in blob
    assert "cloud_t4" in blob


def test_train_loop_shared_between_platforms(rn) -> None:
    """Colab and Kaggle train cells must use the shared train_loop_cell_source."""
    shared_src = rn["shared"].train_loop_cell_source()
    assert "run_autonomous_loop" in shared_src
    colab = rn["pkg"].build_colab_cells()
    kaggle = rn["pkg"].build_kaggle_cells()
    colab_blob = "\n".join(_cell_text(c) for c in colab)
    kaggle_blob = "\n".join(_cell_text(c) for c in kaggle)
    assert shared_src in colab_blob
    assert shared_src in kaggle_blob


def test_install_cells_platform_roots(rn) -> None:
    colab_install = rn["install"]._colab_install_cell()
    kaggle_install = rn["install"]._kaggle_install_cell()
    assert "/content" in colab_install
    assert "cs2_vision_access.training.remote_autonomous" in colab_install
    assert "/kaggle/working" in kaggle_install
    assert "cs2_vision_access.training.remote_autonomous" in kaggle_install


def test_main_writes_notebooks_under_tmp(
    rn, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Drive real main() but redirect output base via chdir + stub tree."""
    out_base = tmp_path / "src" / "cs2_vision_access" / "training" / "notebooks"
    out_base.mkdir(parents=True)
    monkeypatch.chdir(tmp_path)
    rn["pkg"].main()
    colab_path = out_base / "colab.ipynb"
    kaggle_path = out_base / "kaggle.ipynb"
    assert colab_path.is_file()
    assert kaggle_path.is_file()
    colab_doc = json.loads(colab_path.read_text(encoding="utf-8"))
    kaggle_doc = json.loads(kaggle_path.read_text(encoding="utf-8"))
    assert len(colab_doc["cells"]) >= 7
    assert len(kaggle_doc["cells"]) >= 7
    assert colab_doc["nbformat"] == 4
    assert kaggle_doc["nbformat"] == 4


def test_facade_script_loads_main() -> None:
    """Import the real CLI facade and confirm it exposes main."""
    facade = SCRIPTS / "_gen_remote_notebooks.py"
    assert facade.is_file()
    spec = importlib.util.spec_from_file_location("gen_remote_notebooks_facade", facade)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    # Facade inserts scripts/ on path then imports remote_notebooks.main
    spec.loader.exec_module(mod)
    assert callable(mod.main)
