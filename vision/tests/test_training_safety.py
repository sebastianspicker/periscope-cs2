"""Safety / correctness regressions for Vision training workflows (tmp_path only)."""

from __future__ import annotations

import os
import subprocess
import sys
import zipfile
from pathlib import Path

import pytest

from cs2_vision_access.workflows.training.auto.cloud_bundle import materialize_flat_cloud_bundle
from cs2_vision_access.workflows.training.bundle import create_bundle
from cs2_vision_access.workflows.training.cloud.dataset import create_space_dataset_yaml
from cs2_vision_access.workflows.training.dataset_zip import safe_extract_zip, split_holdout
from cs2_vision_access.workflows.training.remote_autonomous.fsutil import (
    _clear_split_dirs,
    _dataset_fingerprint,
)
from cs2_vision_access.workflows.training.remote_autonomous.splits import (
    materialize_held_out_split,
)
from cs2_vision_access.workflows.training.remote_autonomous.state import (
    _matching_prior_state,
    _save_autonomous_state,
)
from cs2_vision_access.workflows.training.self_train.labels import write_label

_VISION_ROOT = Path(__file__).resolve().parents[1]


def _zip(path: Path, members: dict[str, bytes]) -> Path:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
        for name, data in members.items():
            zf.writestr(name, data)
    return path


# --- A1: zip allowlist -------------------------------------------------------


def test_zip_normal_dataset_accepted(tmp_path: Path) -> None:
    archive = _zip(
        tmp_path / "ok.zip",
        {"dataset.yaml": b"names: {}\n", "images/a.jpg": b"x", "labels/a.txt": b"0 0 0 1 1 1\n"},
    )
    out = tmp_path / "out"
    with zipfile.ZipFile(archive) as zf:
        safe_extract_zip(zf, out)
    assert (out / "images" / "a.jpg").is_file()


def test_zip_single_top_level_folder_accepted(tmp_path: Path) -> None:
    archive = _zip(tmp_path / "w.zip", {"ds/images/a.png": b"x", "ds/labels/a.txt": b"\n"})
    with zipfile.ZipFile(archive) as zf:
        safe_extract_zip(zf, tmp_path / "out")


@pytest.mark.parametrize(
    "member",
    ["labels.cache", "ds/labels.cache", "labels/x.cache", "images/x.cache", "evil.py", "../x"],
)
def test_zip_rejects_unexpected_members(tmp_path: Path, member: str) -> None:
    archive = _zip(tmp_path / "bad.zip", {"images/a.jpg": b"x", member: b"boom"})
    out = tmp_path / "out"
    with zipfile.ZipFile(archive) as zf, pytest.raises(ValueError):
        safe_extract_zip(zf, out)
    assert not out.exists() or not any(out.rglob("*.cache"))


# --- A2: no third-party placeholder repo ------------------------------------


def test_no_placeholder_repo_reference_under_vision() -> None:
    needle = "xai" + "-org"
    skip = {".venv", ".git", "__pycache__", ".mypy_cache", ".ruff_cache", ".pytest_cache"}
    hits: list[str] = []
    for root, dirs, files in os.walk(_VISION_ROOT):
        dirs[:] = [d for d in dirs if d not in skip and not d.endswith(".egg-info")]
        for name in files:
            path = Path(root) / name
            try:
                if needle in path.read_text(encoding="utf-8"):
                    hits.append(str(path))
            except (UnicodeDecodeError, OSError):
                continue
    assert hits == []


# --- A10: session-split cloud bundle ----------------------------------------


def _session_tree(root: Path, files: list[str]) -> None:
    """Create ``images/<f>.jpg`` + ``labels/<f>.txt`` for each relative path *f*."""
    for rel in files:
        for kind, ext in (("images", ".jpg"), ("labels", ".txt")):
            target = root / kind / (rel + ext)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"0 0 0 1 1 1\n" if kind == "labels" else b"img")


def test_cloud_bundle_preserves_session_split(tmp_path: Path) -> None:
    split = tmp_path / "split"
    _session_tree(split, ["train/s1/f1", "train/s1/f2", "train/s2/f1", "val/s3/f1"])
    flat, zip_path = materialize_flat_cloud_bundle(split, tmp_path / "cloud", class_names={0: "ct"})
    train = (flat / "train.txt").read_text().split()
    val = (flat / "val.txt").read_text().split()
    assert val == ["./images/val__s3__f1.jpg"]
    assert len(train) == 3 and not set(train) & set(val)
    yaml_text = (flat / "dataset.yaml").read_text()
    assert "train: train.txt" in yaml_text and "val: val.txt" in yaml_text
    with zipfile.ZipFile(zip_path) as zf:
        assert zf.read("val.txt").decode().split() == val
        assert "train: train.txt" in zf.read("dataset.yaml").decode()


def test_cloud_bundle_flat_stem_collision_raises(tmp_path: Path) -> None:
    split = tmp_path / "split"
    _session_tree(split, ["train/a__b/f", "train/a/b__f"])
    with pytest.raises(ValueError, match="collision"):
        materialize_flat_cloud_bundle(split, tmp_path / "cloud", class_names={0: "ct"})


# --- B1: deterministic holdout ----------------------------------------------


def test_split_holdout_every_fifth_is_val() -> None:
    names = [f"f{i}.jpg" for i in range(10)]
    train, val = split_holdout(list(reversed(names)))
    assert val == ["f4.jpg", "f9.jpg"] and len(train) == 8 and not set(train) & set(val)


def test_space_yaml_writes_holdout(tmp_path: Path) -> None:
    _session_tree(tmp_path, [f"f{i}" for i in range(10)])
    yaml_path = create_space_dataset_yaml(tmp_path)
    assert "val: val.txt" in yaml_path.read_text()
    assert (tmp_path / "val.txt").read_text().split() == ["./images/f4.jpg", "./images/f9.jpg"]
    train = set((tmp_path / "train.txt").read_text().split())
    assert len(train) == 8 and not train & set((tmp_path / "val.txt").read_text().split())


def test_space_yaml_tiny_dataset_warns_train_only(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    _session_tree(tmp_path, ["a"])
    create_space_dataset_yaml(tmp_path)
    assert "train-only" in capsys.readouterr().out
    assert (tmp_path / "val.txt").read_text() == (tmp_path / "train.txt").read_text()


def test_create_bundle_includes_holdout_files(tmp_path: Path) -> None:
    _session_tree(tmp_path / "in", [f"f{i}" for i in range(5)])
    create_bundle(tmp_path / "in", tmp_path / "b.zip")
    with zipfile.ZipFile(tmp_path / "b.zip") as zf:
        assert zf.read("val.txt").decode().split() == ["./images/f4.jpg"]
        safe_extract_zip(zf, tmp_path / "out")  # own bundle passes the allowlist


# --- B2: hash-seed independent session split --------------------------------


def test_auto_split_sessions_stable_across_hash_seeds() -> None:
    code = (
        "from cs2_vision_access.workflows.training.session_split import auto_split_sessions;"
        "print(auto_split_sessions([f's{i}' for i in range(12)], seed=7))"
    )
    outputs = {
        subprocess.run(
            [sys.executable, "-c", code],
            env={**os.environ, "PYTHONHASHSEED": seed},
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        for seed in ("1", "2")
    }
    assert len(outputs) == 1


# --- B5: split dirs + resume fingerprint ------------------------------------


def _flat_dataset(root: Path, stems: list[str]) -> None:
    (root / "images").mkdir(parents=True, exist_ok=True)
    (root / "labels").mkdir(parents=True, exist_ok=True)
    for stem in stems:
        (root / "images" / f"{stem}.jpg").write_bytes(b"x")
        (root / "labels" / f"{stem}.txt").write_text("0 0 0 1 1 1\n")


def test_rematerialize_leaves_no_stem_in_both_splits(tmp_path: Path) -> None:
    _flat_dataset(tmp_path, [f"s{i}" for i in range(10)])
    materialize_held_out_split(tmp_path, seed=1)
    materialize_held_out_split(tmp_path, seed=2)  # different split of the same stems
    for kind in ("images", "labels"):
        train = {p.stem for p in (tmp_path / kind / "train").iterdir()}
        val = {p.stem for p in (tmp_path / kind / "val").iterdir()}
        assert train and val and not train & val
    assert len(list((tmp_path / "images" / "train").iterdir())) == 8


def test_clear_split_dirs_only_touches_allowlisted_dirs(tmp_path: Path) -> None:
    _flat_dataset(tmp_path, ["a"])
    (tmp_path / "images" / "train").mkdir()
    (tmp_path / "images" / "train" / "old.jpg").write_bytes(b"x")
    _clear_split_dirs(tmp_path)
    assert not (tmp_path / "images" / "train" / "old.jpg").exists()
    assert (tmp_path / "images" / "a.jpg").is_file()
    assert (tmp_path / "labels" / "a.txt").is_file()


def test_clear_split_dirs_refuses_symlink_escape(tmp_path: Path) -> None:
    data = tmp_path / "data"
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "keep.txt").write_text("keep")
    (data / "labels").mkdir(parents=True)
    (data / "labels" / "train").symlink_to(outside, target_is_directory=True)
    with pytest.raises(ValueError):
        _clear_split_dirs(data)
    assert (outside / "keep.txt").is_file()


def test_resume_state_ignored_when_dataset_differs(tmp_path: Path) -> None:
    ds_a, ds_b = tmp_path / "a", tmp_path / "b"
    _flat_dataset(ds_a, ["x", "y"])
    _flat_dataset(ds_b, ["x", "z"])
    fp_a = _dataset_fingerprint(ds_a / "images", ds_a / "labels")
    fp_b = _dataset_fingerprint(ds_b / "images", ds_b / "labels")
    assert fp_a != fp_b
    state = tmp_path / "autonomous_state.json"
    _save_autonomous_state(state, {"dataset_fingerprint": fp_a, "completed_iters": 2})
    assert _matching_prior_state(state, fp_a) is not None
    assert _matching_prior_state(state, fp_b) is None


# --- B4: human negative labels are preserved --------------------------------


def test_overwrite_pseudo_keeps_empty_human_label(tmp_path: Path) -> None:
    label = tmp_path / "a.txt"
    label.write_text("")
    assert write_label(label, ["0 0 0 1 1 1"], policy="overwrite_pseudo") is False
    assert label.read_text() == ""


def test_overwrite_pseudo_revises_pseudo_labels(tmp_path: Path) -> None:
    sidecar = tmp_path / "a.txt"
    sidecar.write_text("")
    (tmp_path / "a.txt.pseudo").write_text("pseudo\n")
    assert write_label(sidecar, ["0 0 0 1 1 1"], policy="overwrite_pseudo") is True
    header = tmp_path / "b.txt"
    header.write_text("# pseudo\n0 0 0 1 1 1\n")
    assert write_label(header, ["0 0.1 0.1 0.5 0.5 0.9 0.9"], policy="overwrite_pseudo") is True


# --- Review follow-ups -------------------------------------------------------


def test_zip_skips_macos_debris(tmp_path: Path) -> None:
    archive = _zip(
        tmp_path / "finder.zip",
        {
            "images/a.jpg": b"x",
            "labels/a.txt": b"\n",
            "__MACOSX/images/._a.jpg": b"meta",
            "images/.DS_Store": b"meta",
        },
    )
    out = tmp_path / "out"
    with zipfile.ZipFile(archive) as zf:
        safe_extract_zip(zf, out)
    assert (out / "images" / "a.jpg").is_file()
    assert not (out / "__MACOSX").exists() and not (out / "images" / ".DS_Store").exists()


def test_space_yaml_keeps_valid_uploaded_split(tmp_path: Path) -> None:
    _session_tree(tmp_path, [f"f{i}" for i in range(6)])
    (tmp_path / "train.txt").write_text("".join(f"./images/f{i}.jpg\n" for i in range(4)))
    (tmp_path / "val.txt").write_text("./images/f4.jpg\n./images/f5.jpg\n")
    create_space_dataset_yaml(tmp_path)
    assert (tmp_path / "val.txt").read_text().split() == ["./images/f4.jpg", "./images/f5.jpg"]


def test_space_yaml_replaces_split_pointing_outside_images(tmp_path: Path) -> None:
    _session_tree(tmp_path, [f"f{i}" for i in range(5)])
    (tmp_path / "train.txt").write_text("/etc/passwd\n")
    (tmp_path / "val.txt").write_text("./images/f4.jpg\n")
    create_space_dataset_yaml(tmp_path)
    assert "/etc/passwd" not in (tmp_path / "train.txt").read_text()


def test_create_bundle_ignores_lone_prepared_split(tmp_path: Path) -> None:
    _session_tree(tmp_path / "in", [f"f{i}" for i in range(5)])
    (tmp_path / "in" / "train.txt").write_text("/abs/path/f0.jpg\n")  # val.txt missing
    create_bundle(tmp_path / "in", tmp_path / "b.zip")
    with zipfile.ZipFile(tmp_path / "b.zip") as zf:
        assert zf.read("val.txt").decode().split() == ["./images/f4.jpg"]
        assert "/abs/" not in zf.read("train.txt").decode()


def test_fingerprint_ignores_label_rewrites(tmp_path: Path) -> None:
    _flat_dataset(tmp_path, ["a", "b"])
    before = _dataset_fingerprint(tmp_path / "images", tmp_path / "labels")
    (tmp_path / "labels" / "a.txt").write_text("0 0.1 0.1 0.2 0.2 0.3 0.3\n")
    assert _dataset_fingerprint(tmp_path / "images", tmp_path / "labels") == before
