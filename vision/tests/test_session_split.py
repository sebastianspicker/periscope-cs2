from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.training.session_split import (
    auto_plan_from_staging,
    auto_split_sessions,
    discover_session_ids,
    session_looks_valid,
)


class SessionSplitTests(unittest.TestCase):
    def test_auto_split_non_empty_train_val(self) -> None:
        plan = auto_split_sessions(["a", "b", "c", "d", "e"], val_ratio=0.2, seed=1)
        self.assertTrue(plan["train"])
        self.assertTrue(plan["val"])
        self.assertEqual(len(set(plan["train"]) & set(plan["val"])), 0)
        self.assertEqual(sorted(plan["train"] + plan["val"]), sorted(["a", "b", "c", "d", "e"]))

    def test_discover_and_write_plan(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("s1", "s2", "s3"):
                (root / name / "images").mkdir(parents=True)
                (root / name / "labels").mkdir(parents=True)
            sessions = discover_session_ids(root)
            self.assertEqual(sessions, ["s1", "s2", "s3"])
            plan_path = auto_plan_from_staging(root, root / "plan.json", val_ratio=0.34, seed=0)
            payload = json.loads(plan_path.read_text(encoding="utf-8"))
            self.assertIn("train", payload)
            self.assertIn("val", payload)

    def test_discover_flat_staging_sessions(self) -> None:
        """Flat session dirs (top-level images or .txt labels) are accepted."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            flat_imgs = root / "flat_imgs"
            flat_imgs.mkdir()
            (flat_imgs / "frame_0001.jpg").write_bytes(b"\xff\xd8fakejpeg")
            (flat_imgs / "frame_0002.png").write_bytes(b"fakepng")

            flat_labels = root / "flat_labels"
            flat_labels.mkdir()
            (flat_labels / "frame_0001.txt").write_text("0 0.5 0.5 0.1 0.1\n", encoding="utf-8")

            nested = root / "nested_ok"
            (nested / "images").mkdir(parents=True)

            empty = root / "empty_dir"
            empty.mkdir()

            hidden = root / ".hidden_session"
            (hidden / "images").mkdir(parents=True)

            only_other = root / "only_json"
            only_other.mkdir()
            (only_other / "meta.json").write_text("{}", encoding="utf-8")

            sessions = discover_session_ids(root)
            self.assertEqual(sessions, ["flat_imgs", "flat_labels", "nested_ok"])

            self.assertTrue(session_looks_valid(flat_imgs))
            self.assertTrue(session_looks_valid(flat_labels))
            self.assertTrue(session_looks_valid(nested))
            self.assertFalse(session_looks_valid(empty))
            self.assertFalse(session_looks_valid(only_other))


if __name__ == "__main__":
    unittest.main()
