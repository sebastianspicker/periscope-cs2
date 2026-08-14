"""Tests for import-box-dataset (local YOLO det → staging)."""

from __future__ import annotations

import io
import json
import os
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import main
from cs2_vision_access.dataset import (
    IMPORT_RIGHTS_NOTE,
    ImportBoxDatasetError,
    detect_layout,
    import_box_dataset,
)
from cs2_vision_access.frames import SESSION_FILENAME, SessionProvenance

# Minimal valid 1x1 PNG (not decoded by import; extension + bytes only).
_MIN_PNG = (
    b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01"
    b"\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00"
    b"\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82"
)


class ImportBoxDatasetTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.source = self.root / "source"
        self.staging = self.root / "staging"
        self.source.mkdir()
        self.staging.mkdir()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _add_yolo_pair(
        self, stem: str, *, split: str = "train", label: str = "0 0.5 0.5 0.2 0.2\n"
    ) -> None:
        image_dir = self.source / "images" / split
        label_dir = self.source / "labels" / split
        image_dir.mkdir(parents=True, exist_ok=True)
        label_dir.mkdir(parents=True, exist_ok=True)
        (image_dir / f"{stem}.png").write_bytes(_MIN_PNG)
        (label_dir / f"{stem}.txt").write_text(label, encoding="utf-8")

    def _add_flat_pair(self, stem: str, label: str = "0 0.5 0.5 0.2 0.2\n") -> None:
        image_dir = self.source / "images"
        label_dir = self.source / "labels"
        image_dir.mkdir(parents=True, exist_ok=True)
        label_dir.mkdir(parents=True, exist_ok=True)
        (image_dir / f"{stem}.jpg").write_bytes(b"\xff\xd8\xff\xd9")  # tiny JPEG
        (label_dir / f"{stem}.txt").write_text(label, encoding="utf-8")

    def _add_pairs_layout(self, stem: str, label: str = "1 0.4 0.4 0.1 0.1\n") -> None:
        (self.source / f"{stem}.png").write_bytes(_MIN_PNG)
        (self.source / f"{stem}.txt").write_text(label, encoding="utf-8")

    def test_detect_yolo_layout(self) -> None:
        self._add_yolo_pair("a")
        self.assertEqual(detect_layout(self.source), "yolo")

    def test_detect_flat_layout(self) -> None:
        self._add_flat_pair("a")
        self.assertEqual(detect_layout(self.source), "flat")

    def test_detect_pairs_layout(self) -> None:
        self._add_pairs_layout("a")
        self.assertEqual(detect_layout(self.source), "pairs")

    def test_import_yolo_train_split(self) -> None:
        self._add_yolo_pair("frame01")
        self._add_yolo_pair("frame02", split="val")
        summary = import_box_dataset(
            self.source,
            self.staging,
            "imported-keremberke",
            split="train",
            layout="auto",
        )
        self.assertEqual(summary.layout, "yolo")
        self.assertEqual(summary.split, "train")
        self.assertEqual(summary.image_count, 1)
        session = self.staging / "imported-keremberke"
        self.assertTrue((session / "images" / "frame01.png").is_file())
        self.assertTrue((session / "labels" / "frame01.txt").is_file())
        self.assertFalse((session / "images" / "frame02.png").exists())
        provenance = SessionProvenance.load(session / SESSION_FILENAME)
        self.assertEqual(provenance.session_id, "imported-keremberke")
        self.assertEqual(provenance.rights.consent_record, IMPORT_RIGHTS_NOTE)
        self.assertEqual(provenance.frame_policy.saved_frames, 1)
        self.assertIn("audit license", provenance.capture_notes)

    def test_import_flat_layout(self) -> None:
        self._add_flat_pair("box1")
        self._add_flat_pair("box2")
        summary = import_box_dataset(
            self.source,
            self.staging,
            "imported-flat",
            layout="flat",
        )
        self.assertEqual(summary.layout, "flat")
        self.assertIsNone(summary.split)
        self.assertEqual(summary.image_count, 2)
        session = self.staging / "imported-flat"
        self.assertEqual(
            (session / "labels" / "box1.txt").read_text(encoding="utf-8").strip(),
            "0 0.5 0.5 0.2 0.2",
        )

    def test_import_pairs_layout_via_auto(self) -> None:
        self._add_pairs_layout("solo")
        summary = import_box_dataset(
            self.source,
            self.staging,
            "imported-pairs",
            layout="auto",
        )
        self.assertEqual(summary.layout, "pairs")
        self.assertEqual(summary.image_count, 1)
        session = self.staging / "imported-pairs"
        self.assertTrue((session / "images" / "solo.png").is_file())
        self.assertTrue((session / "labels" / "solo.txt").is_file())

    def test_max_images_caps_import(self) -> None:
        self._add_flat_pair("a")
        self._add_flat_pair("b")
        self._add_flat_pair("c")
        summary = import_box_dataset(
            self.source,
            self.staging,
            "capped",
            max_images=2,
        )
        self.assertEqual(summary.image_count, 2)
        self.assertEqual(summary.max_images, 2)

    def test_missing_pair_fails_closed(self) -> None:
        self._add_flat_pair("ok")
        (self.source / "images" / "orphan.png").write_bytes(_MIN_PNG)
        with self.assertRaisesRegex(ImportBoxDatasetError, "missing same-stem label"):
            import_box_dataset(self.source, self.staging, "bad")

    def test_orphan_label_fails_closed(self) -> None:
        self._add_flat_pair("ok")
        (self.source / "labels" / "ghost.txt").write_text("", encoding="utf-8")
        with self.assertRaisesRegex(ImportBoxDatasetError, "orphan labels"):
            import_box_dataset(self.source, self.staging, "bad")

    def test_empty_source_fails_closed(self) -> None:
        with self.assertRaises(ImportBoxDatasetError):
            import_box_dataset(self.source, self.staging, "empty")

    def test_symlink_source_fails_closed(self) -> None:
        real = self.root / "real-source"
        real.mkdir()
        (real / "a.png").write_bytes(_MIN_PNG)
        (real / "a.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
        link = self.root / "linked-source"
        try:
            os.symlink(real, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks unavailable")
        with self.assertRaisesRegex(ImportBoxDatasetError, "symlink"):
            import_box_dataset(link, self.staging, "linked")

    def test_symlink_image_fails_closed(self) -> None:
        self._add_flat_pair("ok")
        target = self.root / "outside.png"
        target.write_bytes(_MIN_PNG)
        link = self.source / "images" / "linked.png"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks unavailable")
        with self.assertRaisesRegex(ImportBoxDatasetError, "symlink"):
            import_box_dataset(self.source, self.staging, "bad")

    def test_overwrite_required_for_nonempty_session(self) -> None:
        self._add_flat_pair("a")
        import_box_dataset(self.source, self.staging, "sess")
        with self.assertRaisesRegex(ImportBoxDatasetError, "not empty"):
            import_box_dataset(self.source, self.staging, "sess")
        summary = import_box_dataset(self.source, self.staging, "sess", overwrite=True)
        self.assertEqual(summary.image_count, 1)

    def test_link_flag_hardlinks_when_possible(self) -> None:
        self._add_flat_pair("a")
        summary = import_box_dataset(
            self.source,
            self.staging,
            "linked-sess",
            link=True,
        )
        dest = self.staging / "linked-sess" / "images" / "a.jpg"
        src = self.source / "images" / "a.jpg"
        self.assertTrue(dest.is_file())
        try:
            same = os.path.samefile(src, dest)
        except OSError:
            same = False
        if same:
            self.assertTrue(summary.linked)
        else:
            # Cross-device or unsupported hardlink → copy fallback.
            self.assertFalse(summary.linked)

    def test_invalid_session_id(self) -> None:
        self._add_flat_pair("a")
        with self.assertRaisesRegex(ImportBoxDatasetError, "path segment"):
            import_box_dataset(self.source, self.staging, "bad/id")


class ImportBoxDatasetCliTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.source = self.root / "source"
        self.staging = self.root / "staging"
        self.source.mkdir()
        self.staging.mkdir()
        images = self.source / "images" / "train"
        labels = self.source / "labels" / "train"
        images.mkdir(parents=True)
        labels.mkdir(parents=True)
        (images / "f.png").write_bytes(_MIN_PNG)
        (labels / "f.txt").write_text("0 0.5 0.5 0.2 0.2\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_cli_import_box_dataset(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "import-box-dataset",
                    "--source-root",
                    str(self.source),
                    "--output-staging",
                    str(self.staging),
                    "--session-id",
                    "imported-keremberke",
                    "--split",
                    "train",
                    "--layout",
                    "auto",
                    "--max-images",
                    "10",
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())
        payload = json.loads(stdout.getvalue())
        self.assertEqual(payload["session_id"], "imported-keremberke")
        self.assertEqual(payload["image_count"], 1)
        self.assertEqual(payload["layout"], "yolo")
        session = self.staging / "imported-keremberke"
        self.assertTrue((session / "images" / "f.png").is_file())
        self.assertTrue((session / SESSION_FILENAME).is_file())

    def test_cli_help_lists_command(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as context:
                main(["import-box-dataset", "--help"])
        self.assertEqual(context.exception.code, 0)
        text = stdout.getvalue() + stderr.getvalue()
        self.assertIn("--source-root", text)
        self.assertIn("--session-id", text)
        self.assertIn("--layout", text)


if __name__ == "__main__":
    unittest.main()
