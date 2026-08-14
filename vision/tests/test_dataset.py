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
        self.add_pair("_validation_negative.png", "", split="val")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def add_pair(self, image: str, label: str, split: str = "train") -> None:
        (self.root / "images" / split / image).write_bytes(b"image")
        label_path = self.root / "labels" / split / Path(image).with_suffix(".txt")
        label_path.write_text(label, encoding="utf-8")

    def write_sessions(self, payload: object, name: str = "sessions.json") -> Path:
        path = self.root / name
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def codes(self) -> list[str]:
        return [issue.code for issue in validate_yolo_segmentation_dataset(self.root, 2).issues]

    def audit_codes(self, **kwargs: object) -> list[str]:
        result = audit_yolo_segmentation_dataset(self.root, 2, **kwargs)  # type: ignore[arg-type]
        return [issue.code for issue in result.issues]

    def test_valid_polygon(self) -> None:
        self.add_pair("player.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n")

        result = validate_yolo_segmentation_dataset(self.root, 2)

        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.image_count, 2)
        self.assertEqual(result.summary.annotation_count, 1)

    def test_empty_label_is_a_valid_negative(self) -> None:
        self.add_pair("empty.jpg", "")

        result = validate_yolo_segmentation_dataset(self.root, 2)

        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.negative_label_count, 2)

    def test_missing_label_is_rejected(self) -> None:
        (self.root / "images" / "train" / "player.webp").write_bytes(b"image")

        self.assertIn("MISSING_LABEL", self.codes())

    def test_malformed_and_invalid_polygons_are_rejected(self) -> None:
        self.add_pair("malformed.png", "0 0.1 0.1 0.2\n")
        self.add_pair("range.png", "0 0.1 0.1 1.2 0.1 0.5 0.9\n")
        self.add_pair("class.png", "2 0.1 0.1 0.8 0.1 0.5 0.9\n")
        self.add_pair("flat.png", "0 0.1 0.2 0.4 0.2 0.8 0.2\n")

        self.assertEqual(
            self.codes(),
            [
                "INVALID_CLASS_ID",
                "DEGENERATE_POLYGON",
                "MALFORMED_POLYGON",
                "COORDINATE_OUT_OF_RANGE",
            ],
        )

    def test_orphan_label_is_rejected(self) -> None:
        (self.root / "labels" / "train" / "orphan.txt").write_text("", encoding="utf-8")

        self.assertIn("ORPHAN_LABEL", self.codes())

    def test_symlinked_file_is_rejected(self) -> None:
        target = self.root / "outside.png"
        target.write_bytes(b"image")
        link = self.root / "images" / "train" / "linked.png"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        self.assertIn("SYMLINKED_PATH", self.codes())

    def test_symlinked_root_is_rejected(self) -> None:
        link = self.root.parent / f"{self.root.name}-link"
        try:
            os.symlink(self.root, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")
        self.addCleanup(lambda: link.unlink(missing_ok=True))

        result = validate_yolo_segmentation_dataset(link, 2)

        self.assertEqual([issue.code for issue in result.issues], ["SYMLINKED_ROOT"])

    def test_empty_required_split_is_rejected(self) -> None:
        (self.root / "images" / "val" / "_validation_negative.png").unlink()
        (self.root / "labels" / "val" / "_validation_negative.txt").unlink()

        self.assertIn("EMPTY_IMAGE_SPLIT", self.codes())

    def test_symlinked_optional_split_is_rejected(self) -> None:
        external = self.root / "external-test-images"
        external.mkdir()
        link = self.root / "images" / "test"
        try:
            os.symlink(external, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")
        (self.root / "labels" / "test").mkdir()

        self.assertIn("SYMLINKED_PATH", self.codes())

    def test_optional_split_regular_files_are_rejected(self) -> None:
        (self.root / "images" / "test").write_text("not a directory", encoding="utf-8")
        (self.root / "labels" / "test").write_text("not a directory", encoding="utf-8")

        self.assertEqual(self.codes().count("INVALID_SPLIT_PATH"), 2)

    def test_session_leakage_fixture_fails_audit(self) -> None:
        self.add_pair("frame_a.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n", split="train")
        self.add_pair("frame_b.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n", split="val")
        self.write_sessions(
            {
                "frame_a": "session-shared",
                "frame_b": "session-shared",
                "_validation_negative": "session-val-only",
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertFalse(result.is_valid)
        self.assertIn("SESSION_LEAKAGE", [issue.code for issue in result.issues])
        self.assertEqual(result.summary.leaked_session_count, 1)
        messages = [issue.message for issue in result.issues if issue.code == "SESSION_LEAKAGE"]
        self.assertEqual(len(messages), 1)
        self.assertIn("session_id 'session-shared' appears in both train and val", messages[0])
        self.assertIn("images/train/frame_a.png", messages[0])
        self.assertIn("images/val/frame_b.png", messages[0])

    def test_disjoint_sessions_pass_audit(self) -> None:
        self.add_pair("frame_a.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n", split="train")
        self.add_pair("frame_b.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n", split="val")
        self.write_sessions(
            [
                {"session_id": "session-train", "images": ["frame_a"]},
                {
                    "session_id": "session-val",
                    "images": ["images/val/frame_b.png", "_validation_negative"],
                },
            ]
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.session_mapped_image_count, 3)
        self.assertEqual(result.summary.train_session_count, 1)
        self.assertEqual(result.summary.val_session_count, 1)
        self.assertEqual(result.summary.leaked_session_count, 0)

    def test_directory_session_mapping_detects_leakage(self) -> None:
        train_dir = self.root / "images" / "train" / "match-01"
        val_dir = self.root / "images" / "val" / "match-01"
        train_dir.mkdir(parents=True)
        val_dir.mkdir(parents=True)
        (self.root / "labels" / "train" / "match-01").mkdir(parents=True)
        (self.root / "labels" / "val" / "match-01").mkdir(parents=True)
        self.add_pair("match-01/a.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n", split="train")
        self.add_pair("match-01/b.png", "", split="val")
        self.write_sessions({"match-01": "match-01"})

        self.assertIn("SESSION_LEAKAGE", self.audit_codes())

    def test_unmapped_image_is_reported_when_sessions_present(self) -> None:
        self.add_pair("mapped.png", "", split="train")
        self.add_pair("unmapped.png", "", split="train")
        self.write_sessions({"mapped": "session-train", "_validation_negative": "session-val"})

        self.assertIn("UNMAPPED_IMAGE", self.audit_codes())

    def test_missing_explicit_sessions_file_is_reported(self) -> None:
        missing = self.root / "missing-sessions.json"

        codes = self.audit_codes(sessions_path=missing)

        self.assertIn("MISSING_SESSIONS_FILE", codes)

    def test_check_decode_skips_without_opencv_or_marks_unreadable(self) -> None:
        self.add_pair("player.png", "0 0.1 0.1 0.8 0.1 0.5 0.9\n")
        self.write_sessions(
            {
                "player": "session-train",
                "_validation_negative": "session-val",
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2, check_decode=True)

        try:
            import cv2  # noqa: F401
        except ImportError:
            self.assertEqual(result.summary.decode_checked_count, 0)
            self.assertEqual(result.summary.decode_skipped_reason, "opencv_unavailable")
            self.assertNotIn("UNREADABLE_IMAGE", [issue.code for issue in result.issues])
            return

        self.assertEqual(result.summary.decode_checked_count, 2)
        self.assertIsNone(result.summary.decode_skipped_reason)
        self.assertIn("UNREADABLE_IMAGE", [issue.code for issue in result.issues])

    def test_default_symlinked_sessions_json_is_rejected(self) -> None:
        self.add_pair("frame_a.png", "", split="train")
        target = self.root / "shared-sessions.json"
        target.write_text(
            json.dumps(
                {
                    "frame_a": "session-shared",
                    "_validation_negative": "session-shared",
                }
            ),
            encoding="utf-8",
        )
        link = self.root / "sessions.json"
        try:
            os.symlink(target, link)
        except (NotImplementedError, OSError):
            self.skipTest("symlinks are unavailable in this environment")

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertFalse(result.is_valid)
        self.assertIn("SYMLINKED_PATH", [issue.code for issue in result.issues])
        self.assertEqual(result.summary.session_mapped_image_count, 0)
        self.assertNotIn("SESSION_LEAKAGE", [issue.code for issue in result.issues])

    def test_train_test_session_leakage_is_reported(self) -> None:
        (self.root / "images" / "test").mkdir()
        (self.root / "labels" / "test").mkdir()
        self.add_pair("frame_a.png", "", split="train")
        self.add_pair("frame_t.png", "", split="test")
        self.write_sessions(
            {
                "frame_a": "session-shared",
                "frame_t": "session-shared",
                "_validation_negative": "session-val",
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertFalse(result.is_valid)
        messages = [issue.message for issue in result.issues if issue.code == "SESSION_LEAKAGE"]
        self.assertTrue(any("train and test" in message for message in messages))
        self.assertIn("images/train/frame_a.png", messages[0])
        self.assertIn("images/test/frame_t.png", messages[0])

    def test_duplicate_basename_stem_does_not_false_leak(self) -> None:
        self.add_pair("0001.png", "", split="train")
        self.add_pair("0001.png", "", split="val")
        self.write_sessions(
            {
                "images/train/0001.png": "session-train",
                "images/val/0001.png": "session-val",
                "_validation_negative": "session-val-only",
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.leaked_session_count, 0)

    def test_single_session_record_object_is_accepted(self) -> None:
        self.add_pair("only.png", "", split="train")
        # Map every image via list-of-one style single object.
        self.write_sessions(
            {
                "session_id": "session-all",
                "images": [
                    "images/train/only.png",
                    "images/val/_validation_negative.png",
                ],
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertFalse(result.is_valid)
        self.assertIn("SESSION_LEAKAGE", [issue.code for issue in result.issues])

    def test_split_name_key_does_not_shadow_nested_session_folder(self) -> None:
        (self.root / "images" / "train" / "match-01").mkdir(parents=True)
        (self.root / "labels" / "train" / "match-01").mkdir(parents=True)
        self.add_pair("match-01/a.png", "", split="train")
        self.write_sessions(
            {
                "train": "whole-train-should-not-apply",
                "match-01": "session-train",
                "_validation_negative": "session-val",
            }
        )

        result = audit_yolo_segmentation_dataset(self.root, 2)

        self.assertTrue(result.is_valid)
        self.assertEqual(result.summary.train_session_count, 1)


if __name__ == "__main__":
    unittest.main()
