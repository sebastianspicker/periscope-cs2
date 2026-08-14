"""Unit tests for automatic labeling helpers in training.prepare / sources."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock

import numpy as np

from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.training.contracts import (
    PRODUCT_CLASSES,
    VOMBIT_CLASSES,
    classes_for_class_map,
    write_dataset_yaml,
)
from cs2_vision_access.training.prepare import parse_class_map_spec
from cs2_vision_access.training.sources import (
    _process_frame,
    _write_dataset_yaml,
    next_frame_index,
    resolve_write_root,
)


def _inst(
    class_id: int,
    confidence: float,
    polygon: tuple[tuple[float, float], ...] | None = None,
) -> InstanceMask:
    if polygon is None:
        # 10x10 square → area 100
        polygon = ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0))
    return InstanceMask(
        frame_index=0,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=f"c{class_id}",
    )


def _mock_segmenter(instances: tuple[InstanceMask, ...]) -> MagicMock:
    seg = MagicMock()
    seg.predict.return_value = instances
    return seg


class NextFrameIndexTests(unittest.TestCase):
    def test_empty_dir_returns_zero(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            self.assertEqual(next_frame_index(root), 0)

    def test_missing_subdirs_returns_zero(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(next_frame_index(Path(tmp)), 0)

    def test_max_from_labels_and_images(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = root / "images"
            labels = root / "labels"
            images.mkdir()
            labels.mkdir()
            (images / "frame_00000002.jpg").write_bytes(b"x")
            (labels / "frame_00000005.txt").write_text("0 0.1 0.1 0.2 0.1 0.15 0.2\n")
            (labels / "notes.txt").write_text("ignore")
            self.assertEqual(next_frame_index(root), 6)

    def test_resume_after_gap_uses_max_plus_one(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            labels = root / "labels"
            labels.mkdir(parents=True)
            (labels / "frame_00000000.txt").write_text("")
            (labels / "frame_00000010.txt").write_text("")
            self.assertEqual(next_frame_index(root), 11)


class ProcessFrameTests(unittest.TestCase):
    def _frame(self) -> np.ndarray:
        return np.zeros((100, 100, 3), dtype=np.uint8)

    def test_confidence_sort_and_max_players(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            # Low conf first, high conf second — max_players=1 should keep high.
            instances = (
                _inst(0, 0.2),
                _inst(1, 0.9),
                _inst(2, 0.5),
            )
            written = _process_frame(
                self._frame(),
                _mock_segmenter(instances),
                frame_index=0,
                output_dir=root,
                max_players=1,
            )
            self.assertEqual(written, 1)
            text = (root / "labels" / "frame_00000000.txt").read_text(encoding="utf-8")
            lines = [ln for ln in text.strip().splitlines() if ln.strip()]
            self.assertEqual(len(lines), 1)
            self.assertTrue(lines[0].startswith("1 "), f"expected class 1 kept, got {lines[0]!r}")

    def test_class_collapse(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            instances = (_inst(0, 0.8), _inst(2, 0.7), _inst(3, 0.6))
            written = _process_frame(
                self._frame(),
                _mock_segmenter(instances),
                frame_index=3,
                output_dir=root,
                max_players=0,
                class_map={0: 0, 1: 0, 2: 0, 3: 0},
            )
            self.assertEqual(written, 1)
            text = (root / "labels" / "frame_00000003.txt").read_text(encoding="utf-8")
            lines = [ln for ln in text.strip().splitlines() if ln.strip()]
            self.assertEqual(len(lines), 3)
            for line in lines:
                self.assertTrue(line.startswith("0 "), line)

    def test_write_empty_negative_always(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            written = _process_frame(
                self._frame(),
                _mock_segmenter(()),
                frame_index=1,
                output_dir=root,
                max_players=0,
                keep_negatives=True,
                negative_every_n=0,
            )
            self.assertEqual(written, 1)
            label = root / "labels" / "frame_00000001.txt"
            image = root / "images" / "frame_00000001.jpg"
            self.assertTrue(label.is_file())
            self.assertTrue(image.is_file())
            self.assertEqual(label.read_text(encoding="utf-8"), "")

    def test_empty_negative_every_n(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            counter = [0]
            seg = _mock_segmenter(())
            results = []
            for i in range(5):
                results.append(
                    _process_frame(
                        self._frame(),
                        seg,
                        frame_index=i,
                        output_dir=root,
                        max_players=0,
                        keep_negatives=True,
                        negative_every_n=5,
                        negative_counter=counter,
                    )
                )
            # Only the 5th empty attempt is written.
            self.assertEqual(results, [0, 0, 0, 0, 1])
            self.assertTrue((root / "labels" / "frame_00000004.txt").is_file())
            self.assertFalse((root / "labels" / "frame_00000000.txt").exists())

    def test_no_negative_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            written = _process_frame(
                self._frame(),
                _mock_segmenter(()),
                frame_index=0,
                output_dir=root,
                max_players=0,
            )
            self.assertEqual(written, 0)
            self.assertEqual(list((root / "labels").glob("*.txt")), [])

    def test_min_confidence_filter(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            instances = (_inst(0, 0.3), _inst(1, 0.8))
            written = _process_frame(
                self._frame(),
                _mock_segmenter(instances),
                frame_index=0,
                output_dir=root,
                max_players=0,
                min_confidence=0.5,
            )
            self.assertEqual(written, 1)
            text = (root / "labels" / "frame_00000000.txt").read_text(encoding="utf-8")
            lines = [ln for ln in text.strip().splitlines() if ln.strip()]
            self.assertEqual(len(lines), 1)
            self.assertTrue(lines[0].startswith("1 "))

    def test_min_mask_area_filter(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "images").mkdir()
            (root / "labels").mkdir()
            tiny = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))  # area 1
            big = ((0.0, 0.0), (20.0, 0.0), (20.0, 20.0), (0.0, 20.0))  # area 400
            instances = (_inst(0, 0.9, tiny), _inst(1, 0.9, big))
            written = _process_frame(
                self._frame(),
                _mock_segmenter(instances),
                frame_index=0,
                output_dir=root,
                max_players=0,
                min_mask_area=50.0,
            )
            self.assertEqual(written, 1)
            text = (root / "labels" / "frame_00000000.txt").read_text(encoding="utf-8")
            lines = [ln for ln in text.strip().splitlines() if ln.strip()]
            self.assertEqual(len(lines), 1)
            self.assertTrue(lines[0].startswith("1 "))


class ClassMapAndYamlTests(unittest.TestCase):
    def test_parse_class_map_spec(self) -> None:
        self.assertEqual(
            parse_class_map_spec("0=0,1=0,2=0,3=0"),
            {0: 0, 1: 0, 2: 0, 3: 0},
        )

    def test_classes_for_class_map_collapse(self) -> None:
        self.assertEqual(
            classes_for_class_map({0: 0, 1: 0, 2: 0, 3: 0}),
            PRODUCT_CLASSES,
        )

    def test_classes_for_class_map_default_vombit(self) -> None:
        self.assertEqual(classes_for_class_map(None), VOMBIT_CLASSES)

    def test_write_dataset_yaml_product(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = write_dataset_yaml(root, classes=PRODUCT_CLASSES)
            text = path.read_text(encoding="utf-8")
            self.assertIn("nc: 1", text)
            self.assertIn("player", text)

    def test_write_dataset_yaml_via_sources_collapse(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_dataset_yaml(root, class_map={0: 0, 1: 0, 2: 0, 3: 0})
            text = (root / "dataset.yaml").read_text(encoding="utf-8")
            self.assertIn("nc: 1", text)
            self.assertIn("player", text)

    def test_write_dataset_yaml_via_sources_default(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_dataset_yaml(root)
            text = (root / "dataset.yaml").read_text(encoding="utf-8")
            self.assertIn("nc: 4", text)
            self.assertIn("ct_head", text)


class SessionLayoutTests(unittest.TestCase):
    def test_resolve_write_root_flat(self) -> None:
        root = Path("data/out")
        self.assertEqual(resolve_write_root(root, None), root)

    def test_resolve_write_root_session(self) -> None:
        root = Path("data/staging")
        self.assertEqual(resolve_write_root(root, "clip-01"), root / "clip-01")


class PrepareCliFlagTests(unittest.TestCase):
    def test_prepare_parser_new_flags(self) -> None:
        from cs2_vision_access.training.prepare import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--video",
                "v.mp4",
                "--detector",
                "d.onnx",
                "--manifest",
                "m.json",
                "--encoder",
                "e.onnx",
                "--decoder",
                "dec.onnx",
                "--keep-negatives",
                "--collapse-to-player",
                "--min-mask-area",
                "12",
                "--min-confidence",
                "0.55",
                "--device",
                "cuda:0",
                "--session-id",
                "s1",
            ]
        )
        self.assertTrue(args.keep_negatives)
        self.assertTrue(args.collapse_to_player)
        self.assertEqual(args.min_mask_area, 12.0)
        self.assertEqual(args.min_confidence, 0.55)
        self.assertEqual(args.device, "cuda:0")
        self.assertEqual(args.session_id, "s1")
        self.assertIsNone(args.negative_every_n)

    def test_batch_parser_new_flags(self) -> None:
        from cs2_vision_access.training.batch import _build_parser

        parser = _build_parser()
        args = parser.parse_args(
            [
                "--sources",
                "a.mp4",
                "--detector",
                "d.onnx",
                "--manifest",
                "m.json",
                "--encoder",
                "e.onnx",
                "--decoder",
                "dec.onnx",
                "--keep-negatives",
                "--negative-every-n",
                "3",
                "--class-map",
                "0=0,1=0,2=0,3=0",
                "--device",
                "cuda",
            ]
        )
        self.assertTrue(args.keep_negatives)
        self.assertEqual(args.negative_every_n, 3)
        self.assertEqual(args.class_map, "0=0,1=0,2=0,3=0")
        self.assertEqual(args.device, "cuda")


if __name__ == "__main__":
    unittest.main()
