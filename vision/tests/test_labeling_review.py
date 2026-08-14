"""Tests for labeling draft review: accept, reject, promote, and CLI."""

from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import main
from cs2_vision_access.labeling import (
    DRAFT_STATUS_FILENAME,
    REVIEW_STATUS_ACCEPTED,
    REVIEW_STATUS_DRAFT_PENDING,
    REVIEW_STATUS_PROMOTED,
    REVIEW_STATUS_REJECTED,
    SCHEMA_VERSION,
    BootstrapError,
    accept_drafts,
    list_draft_status,
    promote_drafts,
    reject_drafts,
)


class ReviewDraftsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.draft_labels = self.root / "labels_draft"
        self.gt_labels = self.root / "labels_gt"
        self.draft_labels.mkdir()
        self._write_draft_fixture()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write_draft_fixture(self) -> None:
        (self.draft_labels / "frame_a.txt").write_text(
            "0 0.1 0.1 0.2 0.1 0.2 0.2 0.1 0.2\n", encoding="utf-8"
        )
        (self.draft_labels / "frame_b.txt").write_text(
            "0 0.3 0.3 0.4 0.3 0.4 0.4 0.3 0.4\n", encoding="utf-8"
        )
        nested = self.draft_labels / "session-a"
        nested.mkdir()
        (nested / "frame_c.txt").write_text("0 0.5 0.5 0.6 0.5 0.6 0.6 0.5 0.6\n", encoding="utf-8")
        self.draft_status = self.draft_labels / DRAFT_STATUS_FILENAME
        payload = {
            "schema_version": SCHEMA_VERSION,
            "review_status": REVIEW_STATUS_DRAFT_PENDING,
            "backend": "rectangle",
            "image_count": 3,
            "label_count": 3,
            "instance_count": 3,
            "negative_count": 0,
            "files": [
                {
                    "image": "frame_a.png",
                    "source_label": "frame_a.txt",
                    "output_label": "frame_a.txt",
                    "instance_count": 1,
                    "review_status": REVIEW_STATUS_DRAFT_PENDING,
                },
                {
                    "image": "frame_b.png",
                    "source_label": "frame_b.txt",
                    "output_label": "frame_b.txt",
                    "instance_count": 1,
                    "review_status": REVIEW_STATUS_DRAFT_PENDING,
                },
                {
                    "image": "session-a/frame_c.png",
                    "source_label": "session-a/frame_c.txt",
                    "output_label": "session-a/frame_c.txt",
                    "instance_count": 1,
                    "review_status": REVIEW_STATUS_DRAFT_PENDING,
                },
            ],
        }
        self.draft_status.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    def test_list_returns_status(self) -> None:
        payload = list_draft_status(self.draft_status)
        self.assertEqual(payload["schema_version"], SCHEMA_VERSION)
        self.assertEqual(len(payload["files"]), 3)

    def test_accept_stem_and_stems(self) -> None:
        summary = accept_drafts(self.draft_status, stem="frame_a")
        self.assertEqual(summary["updated_count"], 1)
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        by_label = {entry["output_label"]: entry for entry in payload["files"]}
        self.assertEqual(by_label["frame_a.txt"]["review_status"], REVIEW_STATUS_ACCEPTED)
        self.assertEqual(by_label["frame_b.txt"]["review_status"], REVIEW_STATUS_DRAFT_PENDING)

        summary = accept_drafts(self.draft_status, stems="frame_b,session-a/frame_c")
        self.assertEqual(summary["updated_count"], 2)
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        self.assertEqual(payload["review_status"], REVIEW_STATUS_ACCEPTED)
        for entry in payload["files"]:
            self.assertEqual(entry["review_status"], REVIEW_STATUS_ACCEPTED)

    def test_accept_all_pending(self) -> None:
        accept_drafts(self.draft_status, stem="frame_a")
        summary = accept_drafts(self.draft_status, all_pending=True)
        self.assertEqual(summary["updated_count"], 2)
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        for entry in payload["files"]:
            self.assertEqual(entry["review_status"], REVIEW_STATUS_ACCEPTED)

    def test_reject_stem(self) -> None:
        summary = reject_drafts(self.draft_status, stem="frame_b")
        self.assertEqual(summary["updated_count"], 1)
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        by_label = {entry["output_label"]: entry for entry in payload["files"]}
        self.assertEqual(by_label["frame_b.txt"]["review_status"], REVIEW_STATUS_REJECTED)

    def test_unknown_stem_fails_closed(self) -> None:
        with self.assertRaisesRegex(BootstrapError, "unknown draft stem"):
            accept_drafts(self.draft_status, stem="missing")

    def test_promote_only_accepted(self) -> None:
        accept_drafts(self.draft_status, stem="frame_a")
        reject_drafts(self.draft_status, stem="frame_b")
        # frame_c remains pending
        summary = promote_drafts(
            self.draft_status,
            self.draft_labels,
            self.gt_labels,
            only_accepted=True,
        )
        self.assertEqual(summary["promoted_count"], 1)
        self.assertEqual(summary["skipped_pending"], 1)
        self.assertEqual(summary["skipped_rejected"], 1)
        self.assertTrue((self.gt_labels / "frame_a.txt").is_file())
        self.assertFalse((self.gt_labels / "frame_b.txt").exists())
        self.assertFalse((self.gt_labels / "session-a" / "frame_c.txt").exists())
        self.assertEqual(
            (self.gt_labels / "frame_a.txt").read_text(encoding="utf-8"),
            (self.draft_labels / "frame_a.txt").read_text(encoding="utf-8"),
        )
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        by_label = {entry["output_label"]: entry for entry in payload["files"]}
        self.assertEqual(by_label["frame_a.txt"]["review_status"], REVIEW_STATUS_PROMOTED)
        self.assertEqual(by_label["frame_b.txt"]["review_status"], REVIEW_STATUS_REJECTED)
        self.assertEqual(
            by_label["session-a/frame_c.txt"]["review_status"], REVIEW_STATUS_DRAFT_PENDING
        )

    def test_promote_nested_path(self) -> None:
        accept_drafts(self.draft_status, stem="session-a/frame_c")
        promote_drafts(
            self.draft_status,
            self.draft_labels,
            self.gt_labels,
            only_accepted=True,
        )
        self.assertTrue((self.gt_labels / "session-a" / "frame_c.txt").is_file())

    def test_promote_never_pending_or_rejected_without_accept(self) -> None:
        summary = promote_drafts(
            self.draft_status,
            self.draft_labels,
            self.gt_labels,
            only_accepted=True,
        )
        self.assertEqual(summary["promoted_count"], 0)
        self.assertEqual(summary["skipped_pending"], 3)
        self.assertFalse(any(self.gt_labels.rglob("*.txt")))

    def test_legacy_payload_without_per_file_status(self) -> None:
        payload = json.loads(self.draft_status.read_text(encoding="utf-8"))
        for entry in payload["files"]:
            entry.pop("review_status", None)
        self.draft_status.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        listed = list_draft_status(self.draft_status)
        for entry in listed["files"]:
            self.assertEqual(entry["review_status"], REVIEW_STATUS_DRAFT_PENDING)
        accept_drafts(self.draft_status, all_pending=True)
        listed = list_draft_status(self.draft_status)
        for entry in listed["files"]:
            self.assertEqual(entry["review_status"], REVIEW_STATUS_ACCEPTED)


class ReviewDraftsCliTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.draft_labels = self.root / "labels_draft"
        self.gt_labels = self.root / "labels_gt"
        self.draft_labels.mkdir()
        (self.draft_labels / "a.txt").write_text("0 0.1 0.1 0.2 0.1 0.2 0.2\n", encoding="utf-8")
        (self.draft_labels / "b.txt").write_text("0 0.3 0.3 0.4 0.3 0.4 0.4\n", encoding="utf-8")
        self.draft_status = self.draft_labels / DRAFT_STATUS_FILENAME
        self.draft_status.write_text(
            json.dumps(
                {
                    "schema_version": SCHEMA_VERSION,
                    "review_status": REVIEW_STATUS_DRAFT_PENDING,
                    "files": [
                        {
                            "image": "a.png",
                            "source_label": "a.txt",
                            "output_label": "a.txt",
                            "instance_count": 1,
                            "review_status": REVIEW_STATUS_DRAFT_PENDING,
                        },
                        {
                            "image": "b.png",
                            "source_label": "b.txt",
                            "output_label": "b.txt",
                            "instance_count": 1,
                            "review_status": REVIEW_STATUS_DRAFT_PENDING,
                        },
                    ],
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_cli_list_accept_reject_promote(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "review-drafts",
                    "list",
                    "--draft-status",
                    str(self.draft_status),
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())
        listed = json.loads(stdout.getvalue())
        self.assertEqual(len(listed["files"]), 2)

        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "review-drafts",
                    "accept",
                    "--draft-status",
                    str(self.draft_status),
                    "--stem",
                    "a",
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())
        accept_payload = json.loads(stdout.getvalue())
        self.assertEqual(accept_payload["updated_count"], 1)

        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "review-drafts",
                    "reject",
                    "--draft-status",
                    str(self.draft_status),
                    "--stem",
                    "b",
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())

        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "review-drafts",
                    "promote",
                    "--draft-status",
                    str(self.draft_status),
                    "--draft-labels-dir",
                    str(self.draft_labels),
                    "--output-labels-dir",
                    str(self.gt_labels),
                    "--only-accepted",
                ]
            )
        self.assertEqual(code, 0, stderr.getvalue())
        promote_payload = json.loads(stdout.getvalue())
        self.assertEqual(promote_payload["promoted_count"], 1)
        self.assertTrue((self.gt_labels / "a.txt").is_file())
        self.assertFalse((self.gt_labels / "b.txt").exists())

    def test_cli_help_lists_subcommands(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as context:
                main(["review-drafts", "--help"])
        self.assertEqual(context.exception.code, 0)
        text = stdout.getvalue() + stderr.getvalue()
        self.assertIn("list", text)
        self.assertIn("accept", text)
        self.assertIn("reject", text)
        self.assertIn("promote", text)


if __name__ == "__main__":
    unittest.main()
