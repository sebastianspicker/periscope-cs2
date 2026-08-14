"""Tests for autonomous training progress reports."""

from __future__ import annotations

import json
import os
import tempfile
import time
import unittest
from pathlib import Path

from cs2_vision_access.training.progress_report import (
    LabelAnalysis,
    analyze_labels,
    find_ultralytics_run_dir,
    parse_ultralytics_results_csv,
    plot_dataset_analysis,
    write_progress_report,
)


def _write_labels(labels_dir: Path) -> None:
    labels_dir.mkdir(parents=True, exist_ok=True)
    # Detection format
    (labels_dir / "a.txt").write_text(
        "0 0.5 0.5 0.2 0.4\n1 0.3 0.7 0.1 0.1\n",
        encoding="utf-8",
    )
    # Segmentation format (triangle → bbox)
    (labels_dir / "b.txt").write_text(
        "0 0.1 0.1 0.9 0.1 0.5 0.9\n",
        encoding="utf-8",
    )
    # Empty label file
    (labels_dir / "c.txt").write_text("", encoding="utf-8")
    # Bad lines mixed with a valid det line
    (labels_dir / "d.txt").write_text(
        "not-a-label\n0 0.2 0.2 0.05 0.05\nbad 1 2\n",
        encoding="utf-8",
    )


class AnalyzeLabelsTests(unittest.TestCase):
    def test_mix_det_and_seg(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp) / "labels"
            _write_labels(labels)
            analysis = analyze_labels(labels, class_names={0: "ct", 1: "t"})

            self.assertEqual(analysis.n_files, 4)
            self.assertEqual(analysis.n_empty, 1)
            # a: cls0, cls1; b: cls0; d: cls0 → class 0: 3, class 1: 1
            self.assertEqual(analysis.class_counts[0], 3)
            self.assertEqual(analysis.class_counts[1], 1)
            self.assertEqual(len(analysis.centers), 4)
            self.assertEqual(len(analysis.sizes), 4)
            self.assertEqual(len(analysis.boxes), 4)

            # Det box center for first instance
            self.assertAlmostEqual(analysis.centers[0][0], 0.5, places=5)
            self.assertAlmostEqual(analysis.centers[0][1], 0.5, places=5)
            self.assertAlmostEqual(analysis.sizes[0][0], 0.2, places=5)
            self.assertAlmostEqual(analysis.sizes[0][1], 0.4, places=5)

            # Seg bbox from triangle 0.1/0.1, 0.9/0.1, 0.5/0.9
            # centers[2] is third instance (seg on b.txt)
            seg_center = analysis.centers[2]
            self.assertAlmostEqual(seg_center[0], 0.5, places=5)
            self.assertAlmostEqual(seg_center[1], 0.5, places=5)
            self.assertAlmostEqual(analysis.sizes[2][0], 0.8, places=5)
            self.assertAlmostEqual(analysis.sizes[2][1], 0.8, places=5)

            # Box xyxy for first det: xc=0.5 w=0.2 → x1=0.4 x2=0.6
            x1, y1, x2, y2, cls = analysis.boxes[0]
            self.assertEqual(cls, 0)
            self.assertAlmostEqual(x1, 0.4, places=5)
            self.assertAlmostEqual(x2, 0.6, places=5)

    def test_missing_dir_returns_empty(self) -> None:
        analysis = analyze_labels(Path("/nonexistent/labels/dir/xyz"))
        self.assertEqual(analysis.n_files, 0)
        self.assertEqual(analysis.class_counts, {})
        self.assertEqual(analysis.unknown_class_counts, {})

    def test_class_names_filters_unknown_ids(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            labels = Path(tmp) / "labels"
            labels.mkdir(parents=True)
            (labels / "a.txt").write_text(
                "0 0.5 0.5 0.2 0.4\n2 0.3 0.3 0.1 0.1\n9 0.1 0.1 0.05 0.05\n",
                encoding="utf-8",
            )
            analysis = analyze_labels(labels, class_names={0: "player", 1: "corpse"})
            self.assertEqual(analysis.class_counts, {0: 1})
            self.assertEqual(analysis.unknown_class_counts, {2: 1, 9: 1})
            self.assertEqual(len(analysis.boxes), 1)
            self.assertEqual(analysis.boxes[0][4], 0)


class PlotDatasetAnalysisTests(unittest.TestCase):
    def test_writes_nonempty_png(self) -> None:
        analysis = LabelAnalysis(
            class_counts={0: 2, 1: 1},
            centers=[(0.5, 0.5), (0.3, 0.7), (0.5, 0.5)],
            sizes=[(0.2, 0.4), (0.1, 0.1), (0.8, 0.8)],
            boxes=[
                (0.4, 0.3, 0.6, 0.7, 0),
                (0.25, 0.65, 0.35, 0.75, 1),
                (0.1, 0.1, 0.9, 0.9, 0),
            ],
            n_files=3,
            n_empty=0,
        )
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "dataset_analysis.png"
            result = plot_dataset_analysis(
                analysis,
                out,
                class_names={0: "ct", 1: "t"},
            )
            self.assertEqual(result, out)
            self.assertTrue(out.is_file())
            self.assertGreater(out.stat().st_size, 1000)
            # PNG magic
            self.assertEqual(out.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")


class ParseResultsCsvTests(unittest.TestCase):
    def test_last_row_and_aliases(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            csv_path = Path(tmp) / "results.csv"
            csv_path.write_text(
                "epoch, train/box_loss, metrics/precision(B), metrics/recall(B), "
                "metrics/mAP50(B), metrics/mAP50-95(B), val/box_loss\n"
                "1, 1.5, 0.1, 0.2, 0.3, 0.15, 1.2\n"
                "2, 1.1, 0.5, 0.6, 0.7, 0.45, 0.9\n",
                encoding="utf-8",
            )
            metrics = parse_ultralytics_results_csv(csv_path)
            self.assertAlmostEqual(metrics["epoch"], 2.0)
            self.assertAlmostEqual(metrics["train/box_loss"], 1.1)
            self.assertAlmostEqual(metrics["val/box_loss"], 0.9)
            self.assertAlmostEqual(metrics["precision"], 0.5)
            self.assertAlmostEqual(metrics["recall"], 0.6)
            self.assertAlmostEqual(metrics["mAP50"], 0.7)
            self.assertAlmostEqual(metrics["mAP50-95"], 0.45)

    def test_missing_file(self) -> None:
        self.assertEqual(parse_ultralytics_results_csv(Path("nope.csv")), {})


class FindRunDirTests(unittest.TestCase):
    def test_finds_newest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            older = root / "exp" / "old"
            newer = root / "exp" / "new"
            older.mkdir(parents=True)
            newer.mkdir(parents=True)
            (older / "results.csv").write_text("epoch\n1\n", encoding="utf-8")
            (newer / "results.csv").write_text("epoch\n2\n", encoding="utf-8")
            # Bump mtime on newer
            now = time.time()
            os.utime(newer / "results.csv", (now + 10, now + 10))
            os.utime(older / "results.csv", (now - 10, now - 10))

            found = find_ultralytics_run_dir(root, name="exp")
            self.assertEqual(found, newer)

            found_all = find_ultralytics_run_dir(root)
            self.assertIsNotNone(found_all)
            self.assertTrue((found_all / "results.csv").is_file())

    def test_finds_nested_runs(self) -> None:
        """Ultralytics-style deep layout: project/name/results.csv under nested runs/."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            # Nested: runs/segment/iter_1 and runs/segment/iter_2
            deep_old = root / "runs" / "segment" / "iter_1"
            deep_new = root / "runs" / "segment" / "iter_2"
            deep_old.mkdir(parents=True)
            deep_new.mkdir(parents=True)
            (deep_old / "results.csv").write_text(
                "epoch,metrics/mAP50(B)\n1,0.1\n", encoding="utf-8"
            )
            (deep_new / "results.csv").write_text(
                "epoch,metrics/mAP50(B)\n2,0.5\n", encoding="utf-8"
            )
            now = time.time()
            os.utime(deep_old / "results.csv", (now - 20, now - 20))
            os.utime(deep_new / "results.csv", (now + 20, now + 20))

            # Search under runs/segment by name
            found = find_ultralytics_run_dir(root / "runs", name="segment")
            self.assertEqual(found, deep_new)

            # Search entire tree — newest nested run wins
            found_all = find_ultralytics_run_dir(root)
            self.assertEqual(found_all, deep_new)

            # Direct hit when results.csv is at project_root/name itself
            direct = root / "project" / "train"
            direct.mkdir(parents=True)
            (direct / "results.csv").write_text("epoch\n1\n", encoding="utf-8")
            self.assertEqual(
                find_ultralytics_run_dir(root / "project", name="train"),
                direct,
            )


class WriteProgressReportTests(unittest.TestCase):
    def test_creates_report_md(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            labels = root / "labels"
            _write_labels(labels)

            run = root / "runs" / "train" / "exp1"
            run.mkdir(parents=True)
            (run / "results.csv").write_text(
                "epoch,metrics/precision(B),metrics/recall(B),metrics/mAP50(B),"
                "metrics/mAP50-95(B),train/box_loss,val/box_loss\n"
                "3,0.8,0.7,0.75,0.5,0.4,0.5\n",
                encoding="utf-8",
            )
            # Tiny fake PNG for curves
            (run / "results.png").write_bytes(b"\x89PNG\r\n\x1a\n" + b"\x00" * 64)

            progress = root / "progress"
            report = write_progress_report(
                progress,
                labels_dir=labels,
                class_names={0: "ct", 1: "t"},
                iterations=[
                    {
                        "iteration": 1,
                        "results_csv": str(run / "results.csv"),
                        "labeled_before": 2,
                        "labeled_after": 4,
                    }
                ],
                title="Test progress",
                notes=["smoke note"],
                images_count=10,
            )

            self.assertEqual(report, progress / "report.md")
            self.assertTrue(report.is_file())
            text = report.read_text(encoding="utf-8")
            self.assertIn("# Test progress", text)
            self.assertIn("dataset_analysis.png", text)
            self.assertIn("mAP50", text)
            self.assertIn("Iteration 1", text)
            self.assertIn("smoke note", text)

            self.assertTrue((progress / "labels_summary.json").is_file())
            self.assertTrue((progress / "dataset_analysis.png").is_file())
            self.assertGreater((progress / "dataset_analysis.png").stat().st_size, 1000)
            self.assertTrue((progress / "metrics_iter_01.json").is_file())
            self.assertTrue((progress / "train_curves_iter_01.png").is_file())

            summary = json.loads((progress / "labels_summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["n_files"], 4)
            self.assertEqual(summary["images_count"], 10)

            metrics = json.loads((progress / "metrics_iter_01.json").read_text(encoding="utf-8"))
            self.assertAlmostEqual(metrics["mAP50"], 0.75)

    def test_results_csv_and_results_png_copy_train_curves(self) -> None:
        """Explicit results_csv + results_png keys produce metrics and curves copy."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            labels = root / "labels"
            _write_labels(labels)

            run = root / "runs" / "iter_1"
            run.mkdir(parents=True)
            csv_path = run / "results.csv"
            csv_path.write_text(
                "epoch,train/box_loss,metrics/mAP50(B)\n1,1.0,0.5\n2,0.8,0.6\n",
                encoding="utf-8",
            )
            # Curves PNG lives outside the run dir so sibling fallback cannot find it
            curves_src = root / "custom_curves.png"
            curves_bytes = b"\x89PNG\r\n\x1a\n" + b"\x01" * 80
            curves_src.write_bytes(curves_bytes)

            progress = root / "progress"
            report = write_progress_report(
                progress,
                labels_dir=labels,
                class_names={0: "player"},
                iterations=[
                    {
                        "iteration": 1,
                        "results_csv": str(csv_path),
                        "results_png": str(curves_src),
                        "labeled_before": 1,
                        "labeled_after": 3,
                    },
                    {
                        "iteration": 2,
                        "results_csv": str(csv_path),
                        # no results_png → no train_curves for iter 2
                        "labeled_before": 3,
                        "labeled_after": 3,
                    },
                ],
                title="Multi-iter progress",
                images_count=5,
            )

            self.assertTrue(report.is_file())
            self.assertTrue((progress / "metrics_iter_01.json").is_file())
            self.assertTrue((progress / "metrics_iter_02.json").is_file())
            curves_dest = progress / "train_curves_iter_01.png"
            self.assertTrue(curves_dest.is_file())
            self.assertEqual(curves_dest.read_bytes(), curves_bytes)
            self.assertFalse((progress / "train_curves_iter_02.png").is_file())

            text = report.read_text(encoding="utf-8")
            self.assertIn("train_curves_iter_01.png", text)
            self.assertIn("Iteration 1", text)
            self.assertIn("Iteration 2", text)
            metrics = json.loads((progress / "metrics_iter_01.json").read_text(encoding="utf-8"))
            self.assertAlmostEqual(metrics["mAP50"], 0.6)
            self.assertAlmostEqual(metrics["epoch"], 2.0)

    def test_empty_labels_still_produces_report_md(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            labels = root / "labels"
            labels.mkdir(parents=True)
            progress = root / "progress"

            report = write_progress_report(
                progress,
                labels_dir=labels,
                class_names={0: "player"},
                iterations=[],
                title="Empty labels progress",
                notes=["no labels yet"],
                images_count=0,
            )

            self.assertEqual(report, progress / "report.md")
            self.assertTrue(report.is_file())
            text = report.read_text(encoding="utf-8")
            self.assertIn("# Empty labels progress", text)
            self.assertIn("dataset_analysis.png", text)
            self.assertIn("no labels yet", text)
            self.assertIn("No labeled instances", text)

            self.assertTrue((progress / "labels_summary.json").is_file())
            self.assertTrue((progress / "dataset_analysis.png").is_file())
            # Empty analysis still yields a real PNG figure
            self.assertGreater((progress / "dataset_analysis.png").stat().st_size, 100)
            summary = json.loads((progress / "labels_summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["n_files"], 0)
            self.assertEqual(summary["n_instances"], 0)


if __name__ == "__main__":
    unittest.main()
