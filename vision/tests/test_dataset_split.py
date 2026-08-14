from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import main
from cs2_vision_access.dataset import audit_yolo_segmentation_dataset
from cs2_vision_access.dataset_split import (
    DatasetSplitError,
    SessionSplitPlan,
    assemble_dataset,
    build_split_plan,
    load_split_plan,
)

POLYGON = "0 0.1 0.1 0.8 0.1 0.5 0.9\n"


class DatasetSplitTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.staging = self.root / "staging"
        self.output = self.root / "cs2_players"
        self.staging.mkdir()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def add_flat_session(
        self,
        session_id: str,
        frames: dict[str, str],
    ) -> Path:
        session_dir = self.staging / session_id
        session_dir.mkdir(parents=True)
        for name, label in frames.items():
            (session_dir / f"{name}.png").write_bytes(b"image")
            (session_dir / f"{name}.txt").write_text(label, encoding="utf-8")
        return session_dir

    def add_nested_session(
        self,
        session_id: str,
        frames: dict[str, str],
    ) -> Path:
        session_dir = self.staging / session_id
        images = session_dir / "images"
        labels = session_dir / "labels"
        images.mkdir(parents=True)
        labels.mkdir(parents=True)
        for name, label in frames.items():
            relative = Path(name)
            image_path = images / relative.with_suffix(".png")
            label_path = labels / relative.with_suffix(".txt")
            image_path.parent.mkdir(parents=True, exist_ok=True)
            label_path.parent.mkdir(parents=True, exist_ok=True)
            image_path.write_bytes(b"image")
            label_path.write_text(label, encoding="utf-8")
        return session_dir

    def test_build_split_plan_rejects_session_in_two_splits(self) -> None:
        with self.assertRaisesRegex(DatasetSplitError, "both train and val"):
            build_split_plan(
                train=["session-a", "session-shared"],
                val=["session-shared"],
            )

    def test_assemble_rejects_hand_built_overlapping_plan(self) -> None:
        self.add_flat_session("shared", {"frame": POLYGON})
        # Bypass build_split_plan: a raw dataclass must still fail closed.
        plan = SessionSplitPlan(train=("shared",), val=("shared",))

        with self.assertRaisesRegex(DatasetSplitError, "both train and val"):
            assemble_dataset(self.staging, self.output, plan)

        self.assertFalse((self.output / "images").exists())
        self.assertFalse((self.output / "labels").exists())
        self.assertFalse((self.output / "sessions.json").exists())

    def test_assemble_copies_whole_sessions_and_keeps_train_val_disjoint(self) -> None:
        self.add_flat_session(
            "session-train",
            {"frame_a": POLYGON, "frame_b": POLYGON, "frame_c": ""},
        )
        self.add_flat_session(
            "session-val",
            {"frame_d": POLYGON, "frame_e": ""},
        )
        self.add_nested_session(
            "session-test",
            {"clip/frame_f": POLYGON},
        )
        plan = build_split_plan(
            train=["session-train"],
            val=["session-val"],
            test=["session-test"],
        )

        summary = assemble_dataset(self.staging, self.output, plan)

        self.assertEqual(summary.image_count, 6)
        self.assertEqual(summary.label_count, 6)
        self.assertEqual(summary.train_sessions, ("session-train",))
        self.assertEqual(summary.val_sessions, ("session-val",))
        self.assertEqual(summary.test_sessions, ("session-test",))

        train_images = sorted(
            path.name for path in (self.output / "images" / "train" / "session-train").iterdir()
        )
        self.assertEqual(train_images, ["frame_a.png", "frame_b.png", "frame_c.png"])
        self.assertTrue(
            (self.output / "labels" / "train" / "session-train" / "frame_a.txt").is_file()
        )
        self.assertTrue(
            (self.output / "images" / "test" / "session-test" / "clip" / "frame_f.png").is_file()
        )

        sessions_payload = json.loads((self.output / "sessions.json").read_text(encoding="utf-8"))
        self.assertEqual(
            sessions_payload,
            {
                "session-test": "session-test",
                "session-train": "session-train",
                "session-val": "session-val",
            },
        )

        audit = audit_yolo_segmentation_dataset(self.output, 1)
        self.assertTrue(audit.is_valid, [issue.code for issue in audit.issues])
        self.assertEqual(audit.summary.leaked_session_count, 0)
        self.assertEqual(audit.summary.train_session_count, 1)
        self.assertEqual(audit.summary.val_session_count, 1)
        self.assertEqual(audit.summary.session_mapped_image_count, 6)

        # Whole-session invariant: no train session_id appears under val paths.
        train_session_dirs = {path.name for path in (self.output / "images" / "train").iterdir()}
        val_session_dirs = {path.name for path in (self.output / "images" / "val").iterdir()}
        self.assertEqual(train_session_dirs & val_session_dirs, set())

    def test_adjacent_frames_stay_in_one_split(self) -> None:
        # Simulate consecutive frames from one recording; plan assigns the whole
        # session to train — none of the adjacent stems may land in val.
        self.add_flat_session(
            "match-01",
            {
                "demo__f000000000": POLYGON,
                "demo__f000000030": POLYGON,
                "demo__f000000060": "",
            },
        )
        self.add_flat_session("holdout", {"only": ""})
        plan = build_split_plan(train=["match-01"], val=["holdout"])

        assemble_dataset(self.staging, self.output, plan)

        for stem in ("demo__f000000000", "demo__f000000030", "demo__f000000060"):
            self.assertTrue(
                (self.output / "images" / "train" / "match-01" / f"{stem}.png").is_file()
            )
            self.assertFalse((self.output / "images" / "val" / "match-01" / f"{stem}.png").exists())

    def test_load_split_plan_and_cli_assemble(self) -> None:
        self.add_flat_session("s-train", {"a": POLYGON})
        self.add_flat_session("s-val", {"b": ""})
        plan_path = self.root / "plan.json"
        plan_path.write_text(
            json.dumps({"train": ["s-train"], "val": ["s-val"]}),
            encoding="utf-8",
        )
        loaded = load_split_plan(plan_path)
        self.assertEqual(loaded.train, ("s-train",))
        self.assertEqual(loaded.val, ("s-val",))

        output = io.StringIO()
        with redirect_stdout(output):
            status = main(
                [
                    "assemble-dataset",
                    "--staging-root",
                    str(self.staging),
                    "--output-root",
                    str(self.output),
                    "--plan",
                    str(plan_path),
                ]
            )

        self.assertEqual(status, 0)
        payload = json.loads(output.getvalue())
        self.assertEqual(payload["image_count"], 2)
        self.assertEqual(payload["train_sessions"], ["s-train"])
        self.assertEqual(payload["val_sessions"], ["s-val"])
        audit = audit_yolo_segmentation_dataset(self.output, 1)
        self.assertTrue(audit.is_valid)

    def test_cli_list_flags_and_rejects_overlap(self) -> None:
        self.add_flat_session("a", {"x": ""})
        self.add_flat_session("b", {"y": ""})
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            status = main(
                [
                    "assemble-dataset",
                    "--staging-root",
                    str(self.staging),
                    "--output-root",
                    str(self.output),
                    "--train",
                    "a,b",
                    "--val",
                    "b",
                ]
            )
        self.assertEqual(status, 2)
        self.assertIn("both train and val", stderr.getvalue())

    def test_missing_label_fails_closed(self) -> None:
        session = self.staging / "broken"
        session.mkdir()
        (session / "frame.png").write_bytes(b"image")
        plan = build_split_plan(train=["broken"], val=["broken-val"])
        # val session present so plan validation passes on lists, then staging fails.
        self.add_flat_session("broken-val", {"z": ""})

        with self.assertRaisesRegex(DatasetSplitError, "missing same-stem label"):
            assemble_dataset(self.staging, self.output, plan)

    def test_overwrite_required_for_existing_output(self) -> None:
        self.add_flat_session("s-train", {"a": ""})
        self.add_flat_session("s-val", {"b": ""})
        plan = build_split_plan(train=["s-train"], val=["s-val"])
        assemble_dataset(self.staging, self.output, plan)

        with self.assertRaisesRegex(DatasetSplitError, "overwrite"):
            assemble_dataset(self.staging, self.output, plan)

        summary = assemble_dataset(self.staging, self.output, plan, overwrite=True)
        self.assertEqual(summary.image_count, 2)

    def test_unassigned_staging_session_is_rejected(self) -> None:
        self.add_flat_session("used-train", {"a": ""})
        self.add_flat_session("used-val", {"b": ""})
        self.add_flat_session("leftover", {"c": ""})
        plan = build_split_plan(train=["used-train"], val=["used-val"])

        with self.assertRaisesRegex(DatasetSplitError, "unassigned sessions"):
            assemble_dataset(self.staging, self.output, plan)


if __name__ == "__main__":
    unittest.main()
