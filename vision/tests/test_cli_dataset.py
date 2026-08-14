from __future__ import annotations

import io
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import main


class CliDatasetTests(unittest.TestCase):
    def test_validate_dataset_returns_one_for_invalid_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "validate-dataset",
                        "--root",
                        temporary_directory,
                        "--class-count",
                        "1",
                    ]
                )

        self.assertEqual(status, 1)
        self.assertIn('"valid": false', output.getvalue())

    def test_audit_dataset_returns_one_for_invalid_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "audit-dataset",
                        "--root",
                        temporary_directory,
                        "--class-count",
                        "1",
                    ]
                )

        self.assertEqual(status, 1)
        payload = output.getvalue()
        self.assertIn('"valid": false', payload)
        self.assertIn('"leaked_session_count"', payload)

    def test_audit_dataset_reports_session_leakage_json(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            for split in ("train", "val"):
                (root / "images" / split).mkdir(parents=True)
                (root / "labels" / split).mkdir(parents=True)
            (root / "images" / "train" / "a.png").write_bytes(b"image")
            (root / "labels" / "train" / "a.txt").write_text("", encoding="utf-8")
            (root / "images" / "val" / "b.png").write_bytes(b"image")
            (root / "labels" / "val" / "b.txt").write_text("", encoding="utf-8")
            (root / "sessions.json").write_text(
                '{"a": "shared", "b": "shared"}',
                encoding="utf-8",
            )
            output = io.StringIO()
            with redirect_stdout(output):
                status = main(
                    [
                        "audit-dataset",
                        "--root",
                        str(root),
                        "--class-count",
                        "1",
                    ]
                )

        self.assertEqual(status, 1)
        payload = output.getvalue()
        self.assertIn('"valid": false', payload)
        self.assertIn("SESSION_LEAKAGE", payload)
        self.assertIn("train and val", payload)
