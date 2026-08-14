from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.cues import (
    SCHEMA_VERSION,
    CueEvent,
    CueLogError,
    CueLogWriter,
    load_cue_events,
    parse_cue_event,
)
from cs2_vision_access.predictions import InstanceMask


def _mask(
    frame_index: int,
    polygon: tuple[tuple[float, float], ...],
    *,
    class_id: int = 0,
    class_name: str = "player",
    confidence: float = 0.9,
) -> InstanceMask:
    return InstanceMask(
        frame_index=frame_index,
        polygon=polygon,
        confidence=confidence,
        class_id=class_id,
        class_name=class_name,
    )


class CueSchemaTests(unittest.TestCase):
    def test_valid_event_round_trip_fields(self) -> None:
        event = CueEvent(
            schema_version=SCHEMA_VERSION,
            event="enter",
            frame_index=3,
            track_id=1,
            class_id=0,
            class_name="player",
            confidence=0.85,
            centroid_x=10.0,
            centroid_y=20.5,
        )
        payload = event.as_json()
        reloaded = parse_cue_event(payload)
        self.assertEqual(reloaded, event)
        self.assertEqual(
            set(payload),
            {
                "schema_version",
                "event",
                "frame_index",
                "track_id",
                "class_id",
                "class_name",
                "confidence",
                "centroid_x",
                "centroid_y",
            },
        )

    def test_unknown_keys_fail_closed(self) -> None:
        raw = {
            "schema_version": SCHEMA_VERSION,
            "event": "enter",
            "frame_index": 0,
            "track_id": 0,
            "class_id": 0,
            "class_name": "player",
            "confidence": 0.5,
            "centroid_x": 1.0,
            "centroid_y": 2.0,
            "download_url": "https://invalid.example/cue",
        }
        with self.assertRaisesRegex(CueLogError, "unknown"):
            parse_cue_event(raw)

    def test_invalid_event_type_and_ranges_fail_closed(self) -> None:
        with self.assertRaisesRegex(CueLogError, "event must be one of"):
            CueEvent(
                schema_version=SCHEMA_VERSION,
                event="pulse",
                frame_index=0,
                track_id=0,
                class_id=0,
                class_name="player",
                confidence=0.5,
                centroid_x=0.0,
                centroid_y=0.0,
            )
        with self.assertRaisesRegex(CueLogError, "confidence"):
            CueEvent(
                schema_version=SCHEMA_VERSION,
                event="leave",
                frame_index=0,
                track_id=0,
                class_id=0,
                class_name="player",
                confidence=1.5,
                centroid_x=0.0,
                centroid_y=0.0,
            )
        with self.assertRaisesRegex(CueLogError, "schema_version"):
            CueEvent(
                schema_version=2,
                event="enter",
                frame_index=0,
                track_id=0,
                class_id=0,
                class_name="player",
                confidence=0.5,
                centroid_x=0.0,
                centroid_y=0.0,
            )


class CueLogWriterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.path = self.root / "cues.jsonl"

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_enter_and_leave_are_emitted_for_track_lifetime(self) -> None:
        square_a = ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0))
        square_b = ((1.0, 1.0), (11.0, 1.0), (11.0, 11.0), (1.0, 11.0))
        with CueLogWriter(self.path) as writer:
            writer.observe(0, [_mask(0, square_a)])
            writer.observe(1, [_mask(1, square_b)])
            writer.observe(2, [])
        events = load_cue_events(self.path)
        self.assertEqual([event.event for event in events], ["enter", "leave"])
        self.assertEqual(events[0].frame_index, 0)
        self.assertEqual(events[0].track_id, 0)
        self.assertEqual(events[1].frame_index, 2)
        self.assertEqual(events[1].track_id, 0)
        self.assertEqual(writer.events_written, 2)

    def test_close_emits_leave_for_active_tracks(self) -> None:
        square = ((0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0))
        writer = CueLogWriter(self.path)
        writer.observe(5, [_mask(5, square)])
        writer.close()
        events = load_cue_events(self.path)
        self.assertEqual([event.event for event in events], ["enter", "leave"])
        self.assertEqual(events[0].frame_index, 5)
        self.assertEqual(events[1].frame_index, 6)

    def test_existing_log_requires_overwrite(self) -> None:
        self.path.write_text("", encoding="utf-8")
        with self.assertRaisesRegex(CueLogError, "already exists"):
            CueLogWriter(self.path)
        CueLogWriter(self.path, overwrite=True).close()
        self.assertTrue(self.path.is_file())
        partial = self.path.with_name(f".{self.path.stem}.partial{self.path.suffix}")
        self.assertFalse(partial.exists())

    def test_failed_overwrite_leaves_existing_destination(self) -> None:
        prior = (
            '{"schema_version":1,"event":"enter","frame_index":0,"track_id":0,'
            '"class_id":0,"class_name":"player","confidence":0.5,'
            '"centroid_x":1.0,"centroid_y":1.0}\n'
        )
        self.path.write_text(prior, encoding="utf-8")
        square = ((0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0))
        writer = CueLogWriter(self.path, overwrite=True)
        writer.observe(0, [_mask(0, square)])
        partial = writer.partial_path
        self.assertTrue(partial.is_file())
        self.assertEqual(self.path.read_text(encoding="utf-8"), prior)
        writer.close(commit=False)
        self.assertFalse(partial.exists())
        self.assertEqual(self.path.read_text(encoding="utf-8"), prior)

    def test_successful_overwrite_replaces_only_on_commit(self) -> None:
        self.path.write_text("stale prior log\n", encoding="utf-8")
        square = ((0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0))
        writer = CueLogWriter(self.path, overwrite=True)
        writer.observe(0, [_mask(0, square)])
        self.assertEqual(self.path.read_text(encoding="utf-8"), "stale prior log\n")
        writer.close(commit=True)
        events = load_cue_events(self.path)
        self.assertEqual(events[0].event, "enter")
        self.assertFalse(writer.partial_path.exists())

    def test_stale_partial_is_rejected(self) -> None:
        partial = self.path.with_name(f".{self.path.stem}.partial{self.path.suffix}")
        partial.write_text("partial\n", encoding="utf-8")
        with self.assertRaisesRegex(CueLogError, "stale partial"):
            CueLogWriter(self.path)

    def test_jsonl_lines_are_schema_valid(self) -> None:
        square = ((2.0, 2.0), (6.0, 2.0), (6.0, 6.0), (2.0, 6.0))
        with CueLogWriter(self.path) as writer:
            writer.observe(0, [_mask(0, square)])
        lines = [
            line for line in self.path.read_text(encoding="utf-8").splitlines() if line.strip()
        ]
        self.assertGreaterEqual(len(lines), 1)
        for line in lines:
            raw = json.loads(line)
            event = parse_cue_event(raw)
            self.assertEqual(event.schema_version, SCHEMA_VERSION)
            self.assertIn(event.event, {"enter", "leave"})


if __name__ == "__main__":
    unittest.main()
