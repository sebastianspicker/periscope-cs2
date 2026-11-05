"""Data-safety tests: assemble/import overwrite, promote, setup, download-model."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import types
from pathlib import Path

import pytest

from cs2_vision_access.application.model_assets.downloads import ModelDownloadError
from cs2_vision_access.interfaces.cli.handlers import download_model, setup
from cs2_vision_access.workflows.dataset import import_boxes
from cs2_vision_access.workflows.dataset.import_boxes import (
    ImportBoxDatasetError,
    import_box_dataset,
)
from cs2_vision_access.workflows.dataset.split import (
    DatasetSplitError,
    assemble_dataset,
    build_split_plan,
)
from cs2_vision_access.workflows.labeling.promote import promote_drafts
from cs2_vision_access.workflows.labeling.review_io import load_draft_status


def _snapshot(root: Path) -> dict[str, bytes]:
    return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob("*") if p.is_file()}


def _session(root: Path, session_id: str) -> None:
    (root / session_id / "images").mkdir(parents=True)
    (root / session_id / "labels").mkdir(parents=True)
    (root / session_id / "images" / "a.jpg").write_bytes(b"img-" + session_id.encode())
    (root / session_id / "labels" / "a.txt").write_text(f"0 {session_id}\n")


def test_assemble_overwrite_failure_keeps_previous_dataset(tmp_path: Path) -> None:
    staging = tmp_path / "staging"
    _session(staging, "s1")
    _session(staging, "s2")
    plan = build_split_plan(train=["s1"], val=["s2"], test=[])
    output = tmp_path / "dataset"
    assemble_dataset(staging, output, plan)
    before = _snapshot(output)

    (staging / "s2" / "labels" / "a.txt").unlink()  # late session now invalid
    with pytest.raises(DatasetSplitError):
        assemble_dataset(staging, output, plan, overwrite=True)

    assert _snapshot(output) == before
    assert sorted(p.name for p in tmp_path.iterdir()) == ["dataset", "staging"]


def test_assemble_overwrite_replaces_dataset(tmp_path: Path) -> None:
    staging = tmp_path / "staging"
    _session(staging, "s1")
    _session(staging, "s2")
    plan = build_split_plan(train=["s1"], val=["s2"], test=[])
    output = tmp_path / "dataset"
    assemble_dataset(staging, output, plan)
    (staging / "s1" / "labels" / "a.txt").write_text("0 new\n")
    (output / "dataset.yaml").write_text("keep")

    assemble_dataset(staging, output, plan, overwrite=True)

    assert (output / "labels" / "train" / "s1" / "a.txt").read_text() == "0 new\n"
    assert (output / "dataset.yaml").read_text() == "keep"
    assert sorted(p.name for p in tmp_path.iterdir()) == ["dataset", "staging"]


def test_import_overwrite_failure_restores_previous_session(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    source = tmp_path / "src"
    (source / "images").mkdir(parents=True)
    (source / "labels").mkdir()
    (source / "images" / "a.jpg").write_bytes(b"x")
    (source / "labels" / "a.txt").write_text("0 0.5 0.5 0.1 0.1\n")
    staging = tmp_path / "staging"
    import_box_dataset(source, staging, "sess")
    before = _snapshot(staging)

    def boom(*_args: object, **_kwargs: object) -> bool:
        raise ImportBoxDatasetError("disk full")

    monkeypatch.setattr(import_boxes, "copy_or_link", boom)
    with pytest.raises(ImportBoxDatasetError):
        import_box_dataset(source, staging, "sess", overwrite=True)

    assert _snapshot(staging) == before
    assert [p.name for p in staging.iterdir()] == ["sess"]


def _promote_fixture(tmp_path: Path) -> tuple[Path, Path, Path]:
    drafts = tmp_path / "drafts"
    drafts.mkdir()
    (drafts / "a.txt").write_text("draft\n")
    status = tmp_path / "draft_status.json"
    status.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "review_status": "accepted",
                "files": [{"output_label": "a.txt", "review_status": "accepted"}],
            }
        )
    )
    return status, drafts, tmp_path / "gt"


def test_promote_keeps_hand_edited_destination_unless_forced(tmp_path: Path) -> None:
    status, drafts, gt = _promote_fixture(tmp_path)
    assert promote_drafts(status, drafts, gt)["promoted_count"] == 1
    (gt / "a.txt").write_text("hand edited\n")

    # Re-accept so the entry is eligible again (as after a re-review).
    payload = json.loads(status.read_text())
    payload["files"][0]["review_status"] = "accepted"
    status.write_text(json.dumps(payload))
    summary = promote_drafts(status, drafts, gt)
    assert summary["skipped_existing"] == ["a"]
    assert summary["promoted_count"] == 0
    assert (gt / "a.txt").read_text() == "hand edited\n"
    assert load_draft_status(status)["files"][0]["review_status"] == "accepted"  # type: ignore[index]

    summary = promote_drafts(status, drafts, gt, force=True)
    assert summary["promoted_count"] == 1
    assert (gt / "a.txt").read_text() == "draft\n"


def test_promote_skips_already_promoted_by_default(tmp_path: Path) -> None:
    status, drafts, gt = _promote_fixture(tmp_path)
    promote_drafts(status, drafts, gt)
    assert promote_drafts(status, drafts, gt)["promoted_count"] == 0


def _setup_args(config: Path, **overrides: object) -> argparse.Namespace:
    values: dict[str, object] = {
        "overlay": None,
        "output_mode": None,
        "device": "cpu",
        "config": config,
        "overwrite_config": False,
    }
    values.update(overrides)
    return argparse.Namespace(**values)


def test_setup_keeps_existing_config_unless_overwrite_flag(tmp_path: Path) -> None:
    config = tmp_path / "cs2-vision-config.json"
    config.write_text('{"custom": true}\n')
    onnx, manifest = tmp_path / "m.onnx", tmp_path / "m.model.json"
    input_config = setup.InputConfig(source_type="screen")

    setup._write_config(_setup_args(config), onnx, manifest, input_config)
    assert config.read_text() == '{"custom": true}\n'

    setup._write_config(_setup_args(config, overwrite_config=True), onnx, manifest, input_config)
    assert config.read_text() != '{"custom": true}\n'


def test_setup_keeps_user_supplied_pt_but_removes_downloaded_one(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    class FakeYOLO:
        def __init__(self, name: str) -> None:
            self.name = name

        def save(self, path: str) -> None:
            Path(path).write_bytes(b"pt")

    monkeypatch.setitem(sys.modules, "ultralytics", types.SimpleNamespace(YOLO=FakeYOLO))

    def fake_export(_pt: Path, onnx: Path, **_kw: object) -> None:
        onnx.write_bytes(b"onnx")

    def fake_manifest(_onnx: Path, manifest: Path, **_kw: object) -> None:
        manifest.write_text("{}")

    monkeypatch.setattr(setup, "export_onnx", fake_export)
    monkeypatch.setattr(setup, "create_manifest", fake_manifest)
    args = argparse.Namespace(model="yolo11n-seg", skip_download=False)

    own = tmp_path / "own"
    own.mkdir()
    (own / "yolo11n-seg.pt").write_bytes(b"mine")
    setup._download_and_export_model(args, own)
    assert (own / "yolo11n-seg.pt").read_bytes() == b"mine"

    fresh = tmp_path / "fresh"
    fresh.mkdir()
    setup._download_and_export_model(args, fresh)
    assert not (fresh / "yolo11n-seg.pt").exists()


def test_download_model_rejects_symlinked_destination(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    def no_network(*_args: object, **_kwargs: object) -> object:
        raise AssertionError("network must not be reached")

    monkeypatch.setattr("urllib.request.build_opener", no_network)
    target = tmp_path / "target.pt"
    target.write_bytes(b"precious")
    dest = tmp_path / "model.pt"
    dest.symlink_to(target)

    with pytest.raises(download_model.DownloadModelError) as info:
        download_model._download_https(
            "https://github.com/x/y.pt", dest, expected_sha256=hashlib.sha256(b"").hexdigest()
        )
    assert isinstance(info.value.__cause__, ModelDownloadError)
    assert target.read_bytes() == b"precious"


def test_assemble_interrupted_swap_restores_previous_dataset(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from cs2_vision_access.workflows.dataset import split as split_module

    staging = tmp_path / "staging"
    _session(staging, "s1")
    _session(staging, "s2")
    plan = build_split_plan(train=["s1"], val=["s2"], test=[])
    output = tmp_path / "dataset"
    assemble_dataset(staging, output, plan)
    before = _snapshot(output)
    (staging / "s1" / "labels" / "a.txt").write_text("0 new\n")

    real_replace = split_module.os.replace
    calls = {"n": 0}

    def interrupting_replace(src: object, dst: object) -> None:
        calls["n"] += 1
        if calls["n"] == 3:  # mid-swap: one entry already moved aside and replaced
            raise KeyboardInterrupt
        real_replace(src, dst)  # type: ignore[arg-type]

    monkeypatch.setattr(split_module.os, "replace", interrupting_replace)
    with pytest.raises(KeyboardInterrupt):
        assemble_dataset(staging, output, plan, overwrite=True)
    monkeypatch.undo()

    assert _snapshot(output) == before
    assert sorted(p.name for p in tmp_path.iterdir()) == ["dataset", "staging"]
