from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from cs2_vision_access.frames import (
    SCHEMA_VERSION,
    SESSION_FILENAME,
    FrameExtractionError,
    SessionProvenance,
    SessionProvenanceError,
    extract_frames,
)


class _FailedCapture:
    def __init__(self) -> None:
        self.released = False

    def isOpened(self) -> bool:
        return False

    def release(self) -> None:
        self.released = True


class _FakeFrame:
    def __init__(self, height: int = 64, width: int = 64) -> None:
        self.shape = (height, width, 3)


class _SequenceCapture:
    def __init__(self, frame_count: int) -> None:
        self._remaining = frame_count
        self.released = False

    def isOpened(self) -> bool:
        return True

    def read(self) -> tuple[bool, _FakeFrame | None]:
        if self._remaining <= 0:
            return False, None
        self._remaining -= 1
        return True, _FakeFrame()

    def release(self) -> None:
        self.released = True


def _fake_cv2(capture: object) -> SimpleNamespace:
    def imwrite(path: str, _frame: object) -> bool:
        Path(path).write_bytes(b"png-fixture")
        return True

    return SimpleNamespace(VideoCapture=lambda _path: capture, imwrite=imwrite)


class FrameExtractionCleanupTests(unittest.TestCase):
    def test_failed_open_releases_capture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "fixture.mp4"
            input_path.write_bytes(b"video fixture")
            capture = _FailedCapture()

            with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
                with self.assertRaisesRegex(FrameExtractionError, "could not open"):
                    extract_frames(
                        input_path,
                        root / "frames",
                        every_n_frames=1,
                        max_saved_frames=1,
                    )

            self.assertTrue(capture.released)
            self.assertFalse((root / "frames" / SESSION_FILENAME).exists())


class FrameExtractionProvenanceTests(unittest.TestCase):
    def test_extract_writes_validated_session_sidecar(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "local-demo.mp4"
            input_path.write_bytes(b"video fixture")
            output = root / "session-001"
            capture = _SequenceCapture(frame_count=5)

            with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
                summary = extract_frames(
                    input_path,
                    output,
                    every_n_frames=2,
                    max_saved_frames=10,
                    capture_notes="offline demo playback",
                )

            self.assertEqual(summary.decoded_frames, 5)
            self.assertEqual(summary.saved_frames, 3)
            self.assertEqual(summary.session_id, "session-001")
            self.assertEqual(summary.source_stem, "local-demo")
            self.assertTrue(capture.released)

            session_path = Path(summary.session_path)
            self.assertEqual(session_path, (output / SESSION_FILENAME).resolve())
            self.assertTrue(session_path.is_file())

            provenance = SessionProvenance.load(session_path)
            self.assertEqual(provenance.schema_version, SCHEMA_VERSION)
            self.assertEqual(provenance.session_id, "session-001")
            self.assertEqual(provenance.source_stem, "local-demo")
            self.assertEqual(provenance.capture_notes, "offline demo playback")
            self.assertEqual(provenance.rights.status, "placeholder")
            self.assertEqual(provenance.rights.source_identifier, "")
            self.assertEqual(provenance.frame_policy.every_n_frames, 2)
            self.assertEqual(provenance.frame_policy.max_saved_frames, 10)
            self.assertEqual(provenance.frame_policy.decoded_frames, 5)
            self.assertEqual(provenance.frame_policy.saved_frames, 3)

            pngs = sorted(path.name for path in output.glob("*.png"))
            self.assertEqual(
                pngs,
                [
                    "local-demo__f000000000.png",
                    "local-demo__f000000002.png",
                    "local-demo__f000000004.png",
                ],
            )

    def test_explicit_session_id_is_written(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "clip.mp4"
            input_path.write_bytes(b"video fixture")
            output = root / "staging"
            capture = _SequenceCapture(frame_count=1)

            with patch.dict(sys.modules, {"cv2": _fake_cv2(capture)}):
                summary = extract_frames(
                    input_path,
                    output,
                    every_n_frames=1,
                    max_saved_frames=1,
                    session_id="match-alpha",
                )

            self.assertEqual(summary.session_id, "match-alpha")
            loaded = SessionProvenance.load(summary.session_path)
            self.assertEqual(loaded.session_id, "match-alpha")

    def test_non_empty_output_directory_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "clip.mp4"
            input_path.write_bytes(b"video fixture")
            output = root / "frames"
            output.mkdir()
            (output / "existing.png").write_bytes(b"x")

            with patch.dict(sys.modules, {"cv2": _fake_cv2(_SequenceCapture(1))}):
                with self.assertRaisesRegex(FrameExtractionError, "must be empty"):
                    extract_frames(
                        input_path,
                        output,
                        every_n_frames=1,
                        max_saved_frames=1,
                    )

    def test_rerun_after_successful_extract_fails_empty_directory_rule(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "clip.mp4"
            input_path.write_bytes(b"video fixture")
            output = root / "session-rerun"

            with patch.dict(sys.modules, {"cv2": _fake_cv2(_SequenceCapture(1))}):
                extract_frames(
                    input_path,
                    output,
                    every_n_frames=1,
                    max_saved_frames=1,
                )
                with self.assertRaisesRegex(FrameExtractionError, "must be empty"):
                    extract_frames(
                        input_path,
                        output,
                        every_n_frames=1,
                        max_saved_frames=1,
                    )


class SessionProvenanceSchemaTests(unittest.TestCase):
    def test_round_trip_payload_validates(self) -> None:
        payload = {
            "schema_version": SCHEMA_VERSION,
            "session_id": "s1",
            "source_stem": "demo",
            "rights": {
                "status": "placeholder",
                "source_identifier": "",
                "consent_record": "",
                "redistribution": "",
                "retention_notes": "",
            },
            "capture_notes": "",
            "frame_policy": {
                "every_n_frames": 30,
                "max_saved_frames": 1000,
                "decoded_frames": 100,
                "saved_frames": 4,
            },
        }
        provenance = SessionProvenance.from_mapping(payload)
        self.assertEqual(provenance.as_json(), payload)

    def test_unknown_key_is_rejected(self) -> None:
        payload = {
            "schema_version": SCHEMA_VERSION,
            "session_id": "s1",
            "source_stem": "demo",
            "rights": {
                "status": "placeholder",
                "source_identifier": "",
                "consent_record": "",
                "redistribution": "",
                "retention_notes": "",
            },
            "capture_notes": "",
            "frame_policy": {
                "every_n_frames": 1,
                "max_saved_frames": 1,
                "decoded_frames": 1,
                "saved_frames": 1,
            },
            "extra": True,
        }
        with self.assertRaisesRegex(SessionProvenanceError, "unknown"):
            SessionProvenance.from_mapping(payload)

    def test_saved_frames_cannot_exceed_decoded(self) -> None:
        payload = {
            "schema_version": SCHEMA_VERSION,
            "session_id": "s1",
            "source_stem": "demo",
            "rights": {
                "status": "placeholder",
                "source_identifier": "",
                "consent_record": "",
                "redistribution": "",
                "retention_notes": "",
            },
            "capture_notes": "",
            "frame_policy": {
                "every_n_frames": 1,
                "max_saved_frames": 10,
                "decoded_frames": 2,
                "saved_frames": 3,
            },
        }
        with self.assertRaisesRegex(SessionProvenanceError, "cannot exceed"):
            SessionProvenance.from_mapping(payload)

    def test_load_rejects_invalid_json_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / SESSION_FILENAME
            path.write_text("{not-json", encoding="utf-8")
            with self.assertRaisesRegex(SessionProvenanceError, "could not read"):
                SessionProvenance.load(path)

    def test_written_sidecar_is_sorted_json(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "clip.mp4"
            input_path.write_bytes(b"video fixture")
            output = root / "sess"
            with patch.dict(sys.modules, {"cv2": _fake_cv2(_SequenceCapture(1))}):
                summary = extract_frames(
                    input_path,
                    output,
                    every_n_frames=1,
                    max_saved_frames=1,
                )
            raw = Path(summary.session_path).read_text(encoding="utf-8")
            parsed = json.loads(raw)
            # sort_keys=True on write; re-dump should match after pretty print.
            expected = json.dumps(parsed, indent=2, sort_keys=True) + "\n"
            self.assertEqual(raw, expected)


if __name__ == "__main__":
    unittest.main()
