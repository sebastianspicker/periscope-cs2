"""Tests for labeling: box geometry, class maps, and boxes-to-masks drafts."""

from __future__ import annotations

import io
import json
import os
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from cs2_vision_access.cli import main
from cs2_vision_access.labeling import (
    DRAFT_STATUS_FILENAME,
    ELLIPSE_POINT_COUNT,
    REVIEW_STATUS_DRAFT_PENDING,
    SCHEMA_VERSION,
    BootstrapError,
    box_to_ellipse_polygon,
    box_to_rectangle_polygon,
    boxes_to_masks,
    format_segmentation_row,
    parse_class_map,
    parse_detection_label,
)


class BoxGeometryTests(unittest.TestCase):
    def test_rectangle_has_four_corners(self) -> None:
        polygon = box_to_rectangle_polygon(0.5, 0.5, 0.4, 0.2)
        self.assertEqual(len(polygon), 8)
        # Top-left, top-right, bottom-right, bottom-left.
        self.assertAlmostEqual(polygon[0], 0.3)
        self.assertAlmostEqual(polygon[1], 0.4)
        self.assertAlmostEqual(polygon[2], 0.7)
        self.assertAlmostEqual(polygon[3], 0.4)
        self.assertAlmostEqual(polygon[4], 0.7)
        self.assertAlmostEqual(polygon[5], 0.6)
        self.assertAlmostEqual(polygon[6], 0.3)
        self.assertAlmostEqual(polygon[7], 0.6)

    def test_ellipse_has_at_least_sixteen_points(self) -> None:
        polygon = box_to_ellipse_polygon(0.5, 0.5, 0.4, 0.2)
        self.assertEqual(len(polygon), ELLIPSE_POINT_COUNT * 2)
        self.assertGreaterEqual(len(polygon) // 2, 16)
        # Points stay inside the axis-aligned box (within float tolerance).
        xs = polygon[0::2]
        ys = polygon[1::2]
        self.assertTrue(all(0.3 - 1e-9 <= x <= 0.7 + 1e-9 for x in xs))
        self.assertTrue(all(0.4 - 1e-9 <= y <= 0.6 + 1e-9 for y in ys))

    def test_format_segmentation_row(self) -> None:
        row = format_segmentation_row(0, [0.1, 0.2, 0.3, 0.2, 0.2, 0.4])
        self.assertTrue(row.startswith("0 "))
        self.assertEqual(len(row.split()), 7)


class ClassMapTests(unittest.TestCase):
    def test_default_maps_all_to_player(self) -> None:
        class_map = parse_class_map(None)
        self.assertEqual(class_map.resolve(0), 0)
        self.assertEqual(class_map.resolve(3), 0)
        self.assertEqual(class_map.resolve(99), 0)

    def test_named_csgo_map(self) -> None:
        class_map = parse_class_map("ct=0,t=0,cthead=0,thead=0")
        self.assertEqual(class_map.resolve(0), 0)
        self.assertEqual(class_map.resolve(1), 0)
        self.assertEqual(class_map.resolve(2), 0)
        self.assertEqual(class_map.resolve(3), 0)
        with self.assertRaisesRegex(BootstrapError, "not listed"):
            class_map.resolve(4)

    def test_numeric_map(self) -> None:
        class_map = parse_class_map("0=0,1=0,2=1")
        self.assertEqual(class_map.resolve(2), 1)

    def test_invalid_entry(self) -> None:
        with self.assertRaisesRegex(BootstrapError, "SOURCE=TARGET"):
            parse_class_map("player")


class DetectionParseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_parse_valid_rows(self) -> None:
        path = self.root / "a.txt"
        path.write_text("0 0.5 0.5 0.2 0.4\n1 0.2 0.3 0.1 0.1\n", encoding="utf-8")
        boxes = parse_detection_label(path)
        self.assertEqual(len(boxes), 2)
        self.assertEqual(boxes[0].source_class_id, 0)
        self.assertAlmostEqual(boxes[0].width, 0.2)

    def test_reject_seg_style_row(self) -> None:
        path = self.root / "bad.txt"
        path.write_text("0 0.1 0.1 0.2 0.1 0.2 0.2\n", encoding="utf-8")
        with self.assertRaisesRegex(BootstrapError, "5 tokens"):
            parse_detection_label(path)

    def test_reject_out_of_range(self) -> None:
        path = self.root / "bad.txt"
        path.write_text("0 0.5 0.5 1.5 0.2\n", encoding="utf-8")
        with self.assertRaisesRegex(BootstrapError, r"\[0, 1\]"):
            parse_detection_label(path)

    def test_reject_zero_size(self) -> None:
        path = self.root / "bad.txt"
        path.write_text("0 0.5 0.5 0.0 0.2\n", encoding="utf-8")
        with self.assertRaisesRegex(BootstrapError, "positive"):
            parse_detection_label(path)


class BoxesToMasksTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.images = self.root / "images"
        self.labels = self.root / "labels"
        self.output = self.root / "output_labels"
        self.images.mkdir()
        self.labels.mkdir()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _add_pair(
        self,
        stem: str,
        label_text: str,
        *,
        nested: str | None = None,
    ) -> None:
        rel = Path(nested) / stem if nested else Path(stem)
        image_path = self.images / rel.with_suffix(".png")
        label_path = self.labels / rel.with_suffix(".txt")
        image_path.parent.mkdir(parents=True, exist_ok=True)
        label_path.parent.mkdir(parents=True, exist_ok=True)
        image_path.write_bytes(b"fake-image")
        label_path.write_text(label_text, encoding="utf-8")

    def test_rectangle_backend_writes_polygons_and_draft_status(self) -> None:
        self._add_pair("frame_a", "0 0.5 0.5 0.2 0.4\n")
        self._add_pair("frame_b", "1 0.3 0.3 0.1 0.1\n2 0.7 0.7 0.05 0.05\n")
        self._add_pair("empty", "")

        summary = boxes_to_masks(
            self.images,
            self.labels,
            self.output,
            backend="rectangle",
        )

        self.assertEqual(summary.image_count, 3)
        self.assertEqual(summary.instance_count, 3)
        self.assertEqual(summary.negative_count, 1)
        self.assertEqual(summary.review_status, REVIEW_STATUS_DRAFT_PENDING)
        self.assertEqual(summary.backend, "rectangle")

        row = (self.output / "frame_a.txt").read_text(encoding="utf-8").strip()
        tokens = row.split()
        self.assertEqual(tokens[0], "0")
        self.assertEqual(len(tokens), 9)  # class + 4 pairs

        multi = (self.output / "frame_b.txt").read_text(encoding="utf-8").strip().splitlines()
        self.assertEqual(len(multi), 2)
        self.assertTrue(all(line.startswith("0 ") for line in multi))

        empty = (self.output / "empty.txt").read_text(encoding="utf-8")
        self.assertEqual(empty, "")

        draft = json.loads((self.output / DRAFT_STATUS_FILENAME).read_text(encoding="utf-8"))
        self.assertEqual(draft["schema_version"], SCHEMA_VERSION)
        self.assertEqual(draft["review_status"], REVIEW_STATUS_DRAFT_PENDING)
        self.assertEqual(draft["backend"], "rectangle")
        self.assertEqual(draft["instance_count"], 3)
        self.assertNotEqual(draft["review_status"], "accepted")
        self.assertNotEqual(draft["review_status"], "ground_truth")
        for entry in draft["files"]:
            self.assertEqual(entry["review_status"], REVIEW_STATUS_DRAFT_PENDING)

    def test_ellipse_backend_point_count(self) -> None:
        self._add_pair("e1", "0 0.5 0.5 0.3 0.3\n")
        boxes_to_masks(
            self.images,
            self.labels,
            self.output,
            backend="ellipse",
        )
        tokens = (self.output / "e1.txt").read_text(encoding="utf-8").split()
        # class_id + 16 pairs
        self.assertEqual(len(tokens), 1 + ELLIPSE_POINT_COUNT * 2)
        self.assertEqual(tokens[0], "0")

    def test_class_map_explicit(self) -> None:
        self._add_pair("m1", "0 0.5 0.5 0.2 0.2\n1 0.2 0.2 0.1 0.1\n")
        boxes_to_masks(
            self.images,
            self.labels,
            self.output,
            class_map="0=0,1=0",
        )
        lines = (self.output / "m1.txt").read_text(encoding="utf-8").strip().splitlines()
        self.assertEqual([line.split()[0] for line in lines], ["0", "0"])

    def test_class_map_rejects_unlisted_source(self) -> None:
        self._add_pair("m1", "5 0.5 0.5 0.2 0.2\n")
        with self.assertRaisesRegex(BootstrapError, "not listed"):
            boxes_to_masks(
                self.images,
                self.labels,
                self.output,
                class_map="ct=0,t=0",
            )

    def test_nested_paths_mirrored(self) -> None:
        self._add_pair("frame", "0 0.5 0.5 0.1 0.1\n", nested="session-a")
        boxes_to_masks(self.images, self.labels, self.output)
        self.assertTrue((self.output / "session-a" / "frame.txt").is_file())

    def test_missing_label_fails_closed(self) -> None:
        (self.images / "lonely.png").write_bytes(b"x")
        with self.assertRaisesRegex(BootstrapError, "no corresponding detection"):
            boxes_to_masks(self.images, self.labels, self.output)

    def test_orphan_label_fails_closed(self) -> None:
        self._add_pair("ok", "0 0.5 0.5 0.1 0.1\n")
        (self.labels / "orphan.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")
        with self.assertRaisesRegex(BootstrapError, "orphan"):
            boxes_to_masks(self.images, self.labels, self.output)

    def test_missing_images_dir_fails_closed(self) -> None:
        with self.assertRaisesRegex(BootstrapError, "images directory"):
            boxes_to_masks(self.root / "missing", self.labels, self.output)

    def test_bad_coords_fail_closed(self) -> None:
        self._add_pair("bad", "0 1.5 0.5 0.1 0.1\n")
        with self.assertRaisesRegex(BootstrapError, r"\[0, 1\]"):
            boxes_to_masks(self.images, self.labels, self.output)

    def test_overwrite_required(self) -> None:
        self._add_pair("a", "0 0.5 0.5 0.1 0.1\n")
        boxes_to_masks(self.images, self.labels, self.output)
        with self.assertRaisesRegex(BootstrapError, "overwrite"):
            boxes_to_masks(self.images, self.labels, self.output)
        summary = boxes_to_masks(
            self.images,
            self.labels,
            self.output,
            overwrite=True,
        )
        self.assertEqual(summary.image_count, 1)

    def test_symlink_images_dir_rejected(self) -> None:
        if not hasattr(os, "symlink"):
            self.skipTest("symlinks unavailable")
        link = self.root / "images_link"
        try:
            link.symlink_to(self.images, target_is_directory=True)
        except OSError:
            self.skipTest("symlink creation failed")
        self._add_pair("a", "0 0.5 0.5 0.1 0.1\n")
        with self.assertRaisesRegex(BootstrapError, "symlink"):
            boxes_to_masks(link, self.labels, self.output)

    def test_sam_without_checkpoint_raises_runtime_error(self) -> None:
        self._add_pair("a", "0 0.5 0.5 0.1 0.1\n")
        env = {key: value for key, value in os.environ.items() if key != "CS2_VISION_SAM_MODEL"}
        with patch.dict(os.environ, env, clear=True):
            with self.assertRaises(RuntimeError) as context:
                boxes_to_masks(
                    self.images,
                    self.labels,
                    self.output,
                    backend="sam",
                )
        message = str(context.exception).lower()
        self.assertTrue(
            "sam" in message
            and (
                "install" in message or "checkpoint" in message or "cs2_vision_sam_model" in message
            )
        )

    def test_sam_import_failure_message(self) -> None:
        self._add_pair("a", "0 0.5 0.5 0.1 0.1\n")
        import cs2_vision_access.labeling as labeling_mod

        with (
            patch.object(
                labeling_mod,
                "_load_sam_impl",
                side_effect=RuntimeError(labeling_mod._SAM_INSTALL_NOTE),
            ),
            self.assertRaisesRegex(RuntimeError, "sam2|SAM|install"),
        ):
            boxes_to_masks(
                self.images,
                self.labels,
                self.output,
                backend="sam",
            )


class BoxesToMasksCliTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.images = self.root / "images"
        self.labels = self.root / "labels"
        self.output = self.root / "out"
        self.images.mkdir()
        self.labels.mkdir()
        (self.images / "f.png").write_bytes(b"img")
        (self.labels / "f.txt").write_text("0 0.5 0.5 0.2 0.2\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_cli_boxes_to_masks(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "boxes-to-masks",
                    "--images-dir",
                    str(self.images),
                    "--labels-dir",
                    str(self.labels),
                    "--output-labels-dir",
                    str(self.output),
                    "--backend",
                    "rectangle",
                    "--class-map",
                    "ct=0,t=0,cthead=0,thead=0",
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())
        payload = json.loads(stdout.getvalue())
        self.assertEqual(payload["review_status"], REVIEW_STATUS_DRAFT_PENDING)
        self.assertEqual(payload["schema_version"], SCHEMA_VERSION)
        self.assertTrue((self.output / "f.txt").is_file())
        draft = json.loads((self.output / DRAFT_STATUS_FILENAME).read_text(encoding="utf-8"))
        self.assertEqual(draft["review_status"], "draft_pending")

    def test_cli_help_lists_command(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as context:
                main(["boxes-to-masks", "--help"])
        self.assertEqual(context.exception.code, 0)
        text = stdout.getvalue() + stderr.getvalue()
        self.assertIn("--images-dir", text)
        self.assertIn("--backend", text)


if __name__ == "__main__":
    unittest.main()
