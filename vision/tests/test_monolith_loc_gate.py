"""Structural gate: no Python file under src/, tests/, scripts/ may exceed 600 LoC."""

from __future__ import annotations

from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
MAX_LOC = 600
SCAN_ROOTS = ("src", "tests", "scripts")
SKIP_PARTS = {"__pycache__", "egg-info", "archive", "build", "dist", ".egg-info"}


def _physical_lines(path: Path) -> int:
    # Physical line count on disk (newline-separated records), matching ledger bar.
    text = path.read_bytes()
    if not text:
        return 0
    # Count lines the same way as File.ReadAllLines / splitlines without dropping last empty.
    return len(text.splitlines())


def _should_skip(path: Path) -> bool:
    parts = set(path.parts)
    if parts & SKIP_PARTS:
        return True
    name = path.name
    return name.endswith(".egg-info") or ".egg-info" in path.parts


def iter_python_files() -> list[Path]:
    found: list[Path] = []
    for root_name in SCAN_ROOTS:
        root = REPO_ROOT / root_name
        if not root.is_dir():
            continue
        for path in root.rglob("*.py"):
            if _should_skip(path):
                continue
            found.append(path)
    return found


def test_no_python_file_exceeds_600_physical_lines() -> None:
    offenders: list[tuple[str, int]] = []
    scanned = 0
    for path in iter_python_files():
        scanned += 1
        n = _physical_lines(path)
        if n > MAX_LOC:
            rel = path.relative_to(REPO_ROOT).as_posix()
            offenders.append((rel, n))
    assert scanned > 0, "expected to scan at least one *.py file"
    if offenders:
        offenders.sort(key=lambda t: t[1], reverse=True)
        detail = "\n".join(f"  {n:4d}  {p}" for p, n in offenders)
        pytest.fail(
            f"{len(offenders)} file(s) exceed {MAX_LOC} physical lines "
            f"(scanned {scanned}):\n{detail}"
        )


def test_scan_covers_scripts_tree() -> None:
    """Ensure scripts/ is part of the gate (regression vs src+tests-only DoD)."""
    scripts = REPO_ROOT / "scripts"
    assert scripts.is_dir()
    py = list(scripts.rglob("*.py"))
    assert any(p.name == "_gen_remote_notebooks.py" for p in py)
    # Former monolith must now be thin.
    facade = scripts / "_gen_remote_notebooks.py"
    assert _physical_lines(facade) <= MAX_LOC
    # Package modules must each be under the gate.
    pkg = scripts / "remote_notebooks"
    assert pkg.is_dir()
    for mod in pkg.glob("*.py"):
        assert _physical_lines(mod) <= MAX_LOC, f"{mod.name} still over {MAX_LOC}"
