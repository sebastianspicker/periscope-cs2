from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.dataset import (
    audit_yolo_segmentation_dataset,
    validate_yolo_segmentation_dataset,
)


class DatasetValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        for split in ("train", "val"):
            (self.root / "images" / split).mkdir(parents=True)
            (self.root / "labels" / split).mkdir(parents=True)
        self.add_pair("_validation_negative.png", "", "val")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def add_pair(self, image: str, label: str, split: str = "train") -> None:
        (self.root / "images" / split / image).write_bytes(b"image")
        (self.root / "labels" / split / Path(image).with_suffix(".txt")).write_text(label)

    def test_valid_baseline(self) -> None:
        self.add_pair("player.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n")
        result = validate_yolo_segmentation_dataset(self.root, 2)
        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.image_count, 2)

    def test_symlink_is_confined(self) -> None:
        target = self.root / "outside.png"
        target.write_bytes(b"image")
        link = self.root / "images" / "train" / "linked.png"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable")
        result = validate_yolo_segmentation_dataset(self.root, 2)
        self.assertIn("SYMLINKED_PATH", [issue.code for issue in result.issues])

    def test_session_leakage_fails_audit(self) -> None:
        self.add_pair("frame_a.png", "", "train")
        self.add_pair("frame_b.png", "", "val")
        (self.root / "sessions.json").write_text(
            json.dumps({"frame_a": "shared", "frame_b": "shared", "_validation_negative": "val"})
        )
        result = audit_yolo_segmentation_dataset(self.root, 2)
        self.assertFalse(result.is_valid)
        self.assertEqual(result.summary.leaked_session_count, 1)
        self.assertIn("SESSION_LEAKAGE", [issue.code for issue in result.issues])
