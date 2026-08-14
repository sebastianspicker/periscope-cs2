"""Unit tests for multi-iter self-train teacher resolution strategies."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from cs2_vision_access.training.teacher_strategy import (
    BestPackageTeacherStrategy,
    PrevStudentTeacherStrategy,
    TeacherPair,
    get_teacher_strategy,
)


class PrevStudentTeacherStrategyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.strategy = PrevStudentTeacherStrategy()

    def test_iter1_with_explicit(self) -> None:
        pair = self.strategy.resolve(
            iteration=1,
            explicit_onnx="models/teacher.onnx",
            explicit_manifest="models/teacher.model.json",
        )
        self.assertIsNotNone(pair)
        assert pair is not None
        self.assertEqual(pair.model, Path("models/teacher.onnx"))
        self.assertEqual(pair.manifest, Path("models/teacher.model.json"))

    def test_iter1_without_explicit_returns_none(self) -> None:
        pair = self.strategy.resolve(iteration=1)
        self.assertIsNone(pair)

    def test_iter2_with_prev_student(self) -> None:
        pair = self.strategy.resolve(
            iteration=2,
            prev_student_onnx="run/student.onnx",
            prev_student_manifest="run/student.model.json",
            explicit_onnx="models/teacher.onnx",
            explicit_manifest="models/teacher.model.json",
        )
        self.assertIsNotNone(pair)
        assert pair is not None
        self.assertEqual(pair.model, Path("run/student.onnx"))
        self.assertEqual(pair.manifest, Path("run/student.model.json"))

    def test_iter2_without_prev_returns_none(self) -> None:
        pair = self.strategy.resolve(
            iteration=2,
            explicit_onnx="models/teacher.onnx",
            explicit_manifest="models/teacher.model.json",
        )
        self.assertIsNone(pair)

    def test_name(self) -> None:
        self.assertEqual(self.strategy.name, "prev_student")


class BestPackageTeacherStrategyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.strategy = BestPackageTeacherStrategy()

    def test_iter1_returns_none_without_explicit(self) -> None:
        pair = self.strategy.resolve(iteration=1)
        self.assertIsNone(pair)

    def test_iter1_with_explicit(self) -> None:
        pair = self.strategy.resolve(
            iteration=1,
            explicit_onnx="t.onnx",
            explicit_manifest="t.model.json",
        )
        self.assertIsNotNone(pair)
        assert pair is not None
        self.assertEqual(pair.model, Path("t.onnx"))

    def test_prefers_best_over_iter_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            best_onnx = root / "best.onnx"
            best_manifest = root / "best.model.json"
            best_onnx.write_bytes(b"best")
            best_manifest.write_text("{}", encoding="utf-8")

            snap_onnx = root / "cs2-yolo11n-seg.iter1.onnx"
            snap_manifest = root / "cs2-yolo11n-seg.iter1.model.json"
            snap_onnx.write_bytes(b"snap")
            snap_manifest.write_text("{}", encoding="utf-8")

            pair = self.strategy.resolve(
                iteration=2,
                best_onnx=best_onnx,
                best_manifest=best_manifest,
                data_dir=root,
            )
            self.assertIsNotNone(pair)
            assert pair is not None
            self.assertEqual(pair.model.resolve(), best_onnx.resolve())
            self.assertEqual(pair.manifest.resolve(), best_manifest.resolve())

    def test_falls_back_to_iter_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            snap_onnx = root / "cs2-yolo11n-seg.iter1.onnx"
            snap_manifest = root / "cs2-yolo11n-seg.iter1.model.json"
            snap_onnx.write_bytes(b"snap")
            snap_manifest.write_text("{}", encoding="utf-8")

            # best paths set but missing on disk → snapshot fallback
            pair = self.strategy.resolve(
                iteration=2,
                best_onnx=root / "missing.onnx",
                best_manifest=root / "missing.model.json",
                data_dir=root,
            )
            self.assertIsNotNone(pair)
            assert pair is not None
            self.assertEqual(pair.model.resolve(), snap_onnx.resolve())
            self.assertEqual(pair.manifest.resolve(), snap_manifest.resolve())

    def test_missing_best_and_snapshot_returns_none(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pair = self.strategy.resolve(iteration=2, data_dir=root)
            self.assertIsNone(pair)

    def test_name(self) -> None:
        self.assertEqual(self.strategy.name, "best_package")


class GetTeacherStrategyTests(unittest.TestCase):
    def test_prev_student_aliases(self) -> None:
        for name in ("prev_student", "auto", "Prev_Student"):
            strat = get_teacher_strategy(name)
            self.assertIsInstance(strat, PrevStudentTeacherStrategy)
            self.assertEqual(strat.name, "prev_student")

    def test_best_package_aliases(self) -> None:
        for name in ("best_package", "remote", "REMOTE"):
            strat = get_teacher_strategy(name)
            self.assertIsInstance(strat, BestPackageTeacherStrategy)
            self.assertEqual(strat.name, "best_package")

    def test_unknown_raises(self) -> None:
        with self.assertRaises(ValueError) as ctx:
            get_teacher_strategy("unknown_strategy")
        self.assertIn("unknown teacher strategy", str(ctx.exception))


class TeacherPairTests(unittest.TestCase):
    def test_frozen(self) -> None:
        pair = TeacherPair(model=Path("a.onnx"), manifest=Path("a.model.json"))
        with self.assertRaises((AttributeError, TypeError)):
            pair.model = Path("b.onnx")  # type: ignore[misc]


if __name__ == "__main__":
    unittest.main()
