from __future__ import annotations

import io
import json
import math
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import main

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "eval_masks"


class EvalMasksCliTests(unittest.TestCase):
    def test_eval_masks_cli_prints_schema_versioned_json(self) -> None:
        output = io.StringIO()
        with redirect_stdout(output):
            status = main(
                [
                    "eval-masks",
                    "--predictions",
                    str(FIXTURES / "predictions.v1.json"),
                    "--dataset-root",
                    str(FIXTURES),
                    "--split",
                    "test",
                ]
            )
        self.assertEqual(status, 0)
        payload = json.loads(output.getvalue())
        self.assertEqual(payload["schema_version"], 1)
        self.assertIn("recall_by_area_bin", payload)
        self.assertTrue(math.isfinite(payload["f1"]))

    def test_eval_masks_cli_writes_optional_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_path = Path(temporary_directory) / "eval-run.v1.json"
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                status = main(
                    [
                        "eval-masks",
                        "--predictions",
                        str(FIXTURES / "predictions.v1.json"),
                        "--dataset-root",
                        str(FIXTURES),
                        "--split",
                        "test",
                        "--output",
                        str(output_path),
                    ]
                )
            self.assertEqual(status, 0)
            self.assertTrue(output_path.is_file())
            on_disk = json.loads(output_path.read_text(encoding="utf-8"))
            self.assertEqual(on_disk["schema_version"], 1)
            printed = json.loads(stdout.getvalue())
            self.assertEqual(printed["output"], str(output_path))


class ExtendedEvalCliTests(unittest.TestCase):
    def test_eval_negatives_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            predictions_path = root / "neg.v1.json"
            predictions_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "images": {
                            "a": {
                                "width": 32,
                                "height": 32,
                                "expected_zero": True,
                                "predictions": [],
                            },
                            "b": {
                                "width": 32,
                                "height": 32,
                                "expected_zero": True,
                                "predictions": [
                                    {
                                        "class_id": 0,
                                        "confidence": 0.5,
                                        "polygon": [
                                            [2.0, 2.0],
                                            [12.0, 2.0],
                                            [12.0, 12.0],
                                            [2.0, 12.0],
                                        ],
                                    }
                                ],
                            },
                        },
                    }
                ),
                encoding="utf-8",
            )
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "eval-negatives",
                        "--predictions",
                        str(predictions_path),
                        "--fps",
                        "30",
                    ]
                )
            self.assertEqual(status, 0)
            payload = json.loads(output.getvalue())
            self.assertEqual(payload["schema_version"], 1)
            self.assertEqual(payload["false_positive_count"], 1)
            self.assertTrue(math.isfinite(payload["false_positives_per_minute"]))

    def test_eval_temporal_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "seq.v1.json"
            path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "frames": [
                            {
                                "frame_index": 0,
                                "width": 32,
                                "height": 32,
                                "predictions": [],
                            },
                            {
                                "frame_index": 1,
                                "width": 32,
                                "height": 32,
                                "predictions": [],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(["eval-temporal", "--sequence", str(path)])
            self.assertEqual(status, 0)
            payload = json.loads(output.getvalue())
            self.assertEqual(payload["schema_version"], 1)
            self.assertEqual(payload["presence_flicker_rate"], 0.0)
            self.assertTrue(math.isfinite(payload["mean_centroid_displacement_px"]))

    def test_eval_comfort_cli(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            frame_path = root / "frame.png"
            _write_solid_png(frame_path, width=48, height=48, rgb=(240, 240, 240))
            predictions_path = root / "preds.v1.json"
            predictions_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "predictions": [
                            {
                                "class_id": 0,
                                "confidence": 0.9,
                                "polygon": [
                                    [12.0, 12.0],
                                    [36.0, 12.0],
                                    [36.0, 36.0],
                                    [12.0, 36.0],
                                ],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "eval-comfort",
                        "--frame",
                        str(frame_path),
                        "--predictions",
                        str(predictions_path),
                        "--outer-color",
                        "#101010",
                        "--inner-color",
                        "#F6FF00",
                    ]
                )
            self.assertEqual(status, 0)
            payload = json.loads(output.getvalue())
            self.assertEqual(payload["schema_version"], 1)
            self.assertIn("clutter_fraction", payload)
            self.assertIn("local_stroke_contrast_ge_3_fraction", payload)
            self.assertTrue(math.isfinite(payload["clutter_fraction"]))


def _write_solid_png(
    path: Path,
    *,
    width: int,
    height: int,
    rgb: tuple[int, int, int],
) -> None:
    """Write an uncompressed-filter solid RGB PNG (stdlib only)."""
    import struct
    import zlib

    red, green, blue = rgb
    raw = bytearray()
    row = bytes((red, green, blue) * width)
    for _ in range(height):
        raw.append(0)
        raw.extend(row)
    compressed = zlib.compress(bytes(raw), level=9)

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", compressed)
        + chunk(b"IEND", b"")
    )
