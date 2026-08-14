"""Tests for export-predictions (schema v1 predictions JSON for eval-masks)."""

from __future__ import annotations

import io
import json
import struct
import tempfile
import unittest
import zlib
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import MagicMock, patch

from cs2_vision_access.cli import build_parser, main
from cs2_vision_access.evaluation import SCHEMA_VERSION, load_predictions_json
from cs2_vision_access.predictions import InstanceMask
from cs2_vision_access.training.export_predictions import (
    ExportPredictionsError,
    collect_image_paths,
    export_predictions,
    instance_to_prediction_dict,
    predictions_payload_from_instances,
    resolve_images_dir,
    run_eval_masks_if_available,
)


def _mask(
    polygon: tuple[tuple[float, float], ...],
    *,
    confidence: float = 0.9,
    class_id: int = 0,
    class_name: str = "player",
    frame_index: int = 0,
) -> InstanceMask:
    return InstanceMask(
        frame_index=frame_index,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=class_name,
    )


SQUARE = ((10.0, 10.0), (40.0, 10.0), (40.0, 40.0), (10.0, 40.0))


def _write_minimal_png(path: Path, *, width: int = 32, height: int = 24) -> None:
    """Write a solid gray 8-bit RGB PNG via the stdlib (no OpenCV required)."""
    raw_rows = bytearray()
    row = bytes([0] + [128, 128, 128] * width)
    for _ in range(height):
        raw_rows.extend(row)
    compressed = zlib.compress(bytes(raw_rows), level=9)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", compressed)
        + chunk(b"IEND", b"")
    )


class ResolveImagesDirTests(unittest.TestCase):
    def test_prefers_split_subdirectory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            split_dir = root / "images" / "val"
            split_dir.mkdir(parents=True)
            (root / "images" / "other.png").write_bytes(b"x")
            resolved = resolve_images_dir(root, "val")
            self.assertEqual(resolved, split_dir)

    def test_flat_split_uses_images_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            images = root / "images"
            images.mkdir()
            for split in ("all", "flat", ""):
                with self.subTest(split=split):
                    self.assertEqual(resolve_images_dir(root, split), images)

    def test_missing_split_raises(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "images").mkdir()
            with self.assertRaises(ExportPredictionsError):
                resolve_images_dir(root, "val")


class CollectImagePathsTests(unittest.TestCase):
    def test_collects_sorted_image_files_with_cap(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            images = Path(temporary_directory)
            for name in ("b.png", "a.jpg", "skip.txt", "c.webp"):
                (images / name).write_bytes(b"x")
            paths = collect_image_paths(images, max_images=2)
            self.assertEqual([p.name for p in paths], ["a.jpg", "b.png"])


class PayloadSchemaTests(unittest.TestCase):
    def test_instance_to_prediction_dict_keys(self) -> None:
        payload = instance_to_prediction_dict(_mask(SQUARE, confidence=0.75, class_id=1))
        self.assertEqual(set(payload), {"class_id", "confidence", "polygon"})
        self.assertEqual(payload["class_id"], 1)
        self.assertEqual(payload["confidence"], 0.75)
        self.assertEqual(len(payload["polygon"]), 4)
        self.assertEqual(payload["polygon"][0], [10.0, 10.0])

    def test_predictions_payload_schema_version_and_images(self) -> None:
        payload = predictions_payload_from_instances(
            [
                ("frame_a", 64, 48, (_mask(SQUARE),)),
                ("frame_b", 64, 48, ()),
            ]
        )
        self.assertEqual(payload["schema_version"], SCHEMA_VERSION)
        self.assertEqual(payload["default_width"], 64)
        self.assertEqual(payload["default_height"], 48)
        self.assertIn("frame_a", payload["images"])
        self.assertIn("frame_b", payload["images"])
        entry = payload["images"]["frame_a"]
        self.assertEqual(entry["width"], 64)
        self.assertEqual(entry["height"], 48)
        self.assertEqual(len(entry["predictions"]), 1)
        self.assertEqual(entry["predictions"][0]["class_id"], 0)
        self.assertEqual(payload["images"]["frame_b"]["predictions"], [])


class ExportPredictionsTests(unittest.TestCase):
    def test_export_with_mock_segmenter_writes_schema_keys(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            images_dir = root / "images" / "val"
            images_dir.mkdir(parents=True)
            _write_minimal_png(images_dir / "alpha.png", width=32, height=24)
            _write_minimal_png(images_dir / "beta.png", width=32, height=24)

            mock = MagicMock()
            mock.predict.side_effect = [
                (_mask(SQUARE, confidence=0.88),),
                (),
            ]
            output = root / "preds.json"
            result_path = export_predictions(
                model_path=root / "missing.onnx",
                manifest_path=root / "missing.model.json",
                dataset_root=root,
                split="val",
                output_json=output,
                conf=0.25,
                segmenter=mock,
            )
            self.assertEqual(result_path, output)
            self.assertTrue(output.is_file())

            payload = load_predictions_json(output)
            self.assertEqual(payload["schema_version"], 1)
            self.assertIn("images", payload)
            self.assertEqual(set(payload["images"]), {"alpha", "beta"})
            alpha = payload["images"]["alpha"]
            self.assertEqual(alpha["width"], 32)
            self.assertEqual(alpha["height"], 24)
            self.assertEqual(len(alpha["predictions"]), 1)
            pred = alpha["predictions"][0]
            self.assertEqual(set(pred), {"class_id", "confidence", "polygon"})
            self.assertEqual(pred["confidence"], 0.88)
            self.assertEqual(payload["images"]["beta"]["predictions"], [])
            self.assertEqual(mock.predict.call_count, 2)

    def test_max_images_limits_predict_calls(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            images_dir = root / "images" / "val"
            images_dir.mkdir(parents=True)
            for name in ("a.png", "b.png", "c.png"):
                _write_minimal_png(images_dir / name)

            mock = MagicMock()
            mock.predict.return_value = ()
            output = root / "preds.json"
            export_predictions(
                model_path=root / "m.onnx",
                manifest_path=root / "m.json",
                dataset_root=root,
                split="val",
                output_json=output,
                max_images=1,
                segmenter=mock,
            )
            self.assertEqual(mock.predict.call_count, 1)
            payload = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(list(payload["images"]), ["a"])

    def test_conf_out_of_range_raises(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            with self.assertRaises(ExportPredictionsError):
                export_predictions(
                    model_path=root / "m.onnx",
                    manifest_path=root / "m.json",
                    dataset_root=root,
                    output_json=root / "out.json",
                    conf=1.5,
                    segmenter=MagicMock(),
                )


class RunEvalMasksIfAvailableTests(unittest.TestCase):
    def test_returns_metrics_for_fixture_predictions(self) -> None:
        fixtures = Path(__file__).resolve().parent / "fixtures" / "eval_masks"
        metrics = run_eval_masks_if_available(
            fixtures / "predictions.v1.json",
            fixtures,
            "test",
            out_json=None,
        )
        self.assertIsNotNone(metrics)
        assert metrics is not None
        self.assertEqual(metrics["schema_version"], 1)
        self.assertIn("f1", metrics)


class ExportPredictionsCliTests(unittest.TestCase):
    def test_parser_lists_export_predictions(self) -> None:
        parser = build_parser()
        command_names: set[str] = set()
        for action in parser._actions:
            if getattr(action, "choices", None):
                command_names.update(action.choices)
        self.assertIn("export-predictions", command_names)

    def test_export_predictions_cli_with_mock_segmenter(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            images_dir = root / "images" / "val"
            images_dir.mkdir(parents=True)
            _write_minimal_png(images_dir / "frame.png", width=16, height=16)
            model = root / "model.onnx"
            manifest = root / "model.model.json"
            model.write_bytes(b"fake")
            manifest.write_text("{}", encoding="utf-8")
            output = root / "preds.json"

            mock = MagicMock()
            mock.predict.return_value = (_mask(SQUARE, confidence=0.5),)

            stdout = io.StringIO()
            with (
                patch(
                    "cs2_vision_access.training.export_predictions.create_segmenter",
                    return_value=mock,
                ),
                redirect_stdout(stdout),
            ):
                status = main(
                    [
                        "export-predictions",
                        "--model",
                        str(model),
                        "--manifest",
                        str(manifest),
                        "--dataset-root",
                        str(root),
                        "--split",
                        "val",
                        "--output",
                        str(output),
                        "--device",
                        "cpu",
                        "--conf",
                        "0.25",
                    ]
                )
            self.assertEqual(status, 0)
            printed = json.loads(stdout.getvalue())
            self.assertEqual(printed["output"], str(output))
            payload = load_predictions_json(output)
            self.assertEqual(payload["schema_version"], 1)
            self.assertIn("frame", payload["images"])
            self.assertEqual(len(payload["images"]["frame"]["predictions"]), 1)


if __name__ == "__main__":
    unittest.main()
