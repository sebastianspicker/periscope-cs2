"""Unit tests for offline backend bakeoff (mocked process_video / segmenter)."""

from __future__ import annotations

import json
import math
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import MagicMock

from cs2_vision_access.bakeoff import (
    SCHEMA_VERSION,
    BakeoffBackendSpec,
    BakeoffError,
    load_bakeoff_config,
    parse_backends_csv,
    row_from_summary,
    run_bakeoff,
    specs_from_cli_pairs,
    specs_from_config,
    write_bakeoff_json,
)
from cs2_vision_access.cli import build_parser, main
from cs2_vision_access.video import VideoRunSummary


def _summary(
    *,
    backend: str = "ultralytics-onnx",
    frames: int = 10,
    predicted: int = 4,
    outlined: int = 3,
    inf_p50: float = 12.5,
    inf_p95: float = 18.0,
    pipe_p50: float = 15.0,
    pipe_p95: float = 22.0,
    misses: int = 1,
    reason: str = "max_frames",
) -> VideoRunSummary:
    return VideoRunSummary(
        frames_processed=frames,
        instances_predicted=predicted,
        instances_outlined=outlined,
        stale_predictions_discarded=0,
        degenerate_masks_discarded=0,
        source_fps=30.0,
        source_width=1280,
        source_height=720,
        frame_budget_ms=1000.0 / 30.0,
        frame_budget_misses=misses,
        elapsed_seconds=1.0,
        throughput_fps=10.0,
        inference_ms_p50=inf_p50,
        inference_ms_p95=inf_p95,
        pipeline_ms_p50=pipe_p50,
        pipeline_ms_p95=pipe_p95,
        completed_stream=False,
        termination_reason=reason,
        outline_inner_color="#F6FF00",
        outline_outer_color="#101010",
        outline_fill_opacity=0.08,
        outline_inner_width_pixels=3,
        outline_outer_width_pixels=7,
        outline_stroke_contrast_ratio=12.0,
        output_path=None,
        segmenter_backend=backend,
    )


class BakeoffSpecTests(unittest.TestCase):
    def test_empty_backends_list_fails_closed(self) -> None:
        with self.assertRaisesRegex(BakeoffError, "empty"):
            parse_backends_csv("")
        with self.assertRaisesRegex(BakeoffError, "empty"):
            parse_backends_csv("  ,  ")
        with self.assertRaisesRegex(BakeoffError, "empty"):
            specs_from_cli_pairs(
                [],
                model_a="a.onnx",
                manifest_a="a.json",
            )
        with self.assertRaisesRegex(BakeoffError, "empty"):
            specs_from_config({"backends": []})
        with self.assertRaisesRegex(BakeoffError, "empty"):
            run_bakeoff(
                input_path="clip.mp4",
                backends=(),
                output_path=None,
            )

    def test_unknown_backend_fails_closed(self) -> None:
        with self.assertRaisesRegex(BakeoffError, "unknown"):
            BakeoffBackendSpec(
                backend="not-a-backend",
                model=Path("m.onnx"),
                manifest=Path("m.json"),
            )

    def test_rf_detr_alias_normalizes(self) -> None:
        spec = BakeoffBackendSpec(
            backend="rf-detr",
            model=Path("m.onnx"),
            manifest=Path("m.json"),
        )
        self.assertEqual(spec.backend, "rfdetr")

    def test_cli_pairs_require_matching_paths(self) -> None:
        with self.assertRaisesRegex(BakeoffError, "model-a"):
            specs_from_cli_pairs(
                ["ultralytics-onnx"],
                model_a=None,
                manifest_a=None,
            )
        with self.assertRaisesRegex(BakeoffError, "model-b"):
            specs_from_cli_pairs(
                ["ultralytics-onnx", "rfdetr"],
                model_a="a.onnx",
                manifest_a="a.json",
                model_b=None,
                manifest_b=None,
            )

    def test_cli_pairs_two_backends(self) -> None:
        specs = specs_from_cli_pairs(
            ["ultralytics-onnx", "rfdetr"],
            model_a="yolo.onnx",
            manifest_a="yolo.json",
            model_b="rf.onnx",
            manifest_b="rf.json",
        )
        self.assertEqual(len(specs), 2)
        self.assertEqual(specs[0].backend, "ultralytics-onnx")
        self.assertEqual(specs[1].backend, "rfdetr")
        self.assertEqual(specs[0].model, Path("yolo.onnx"))
        self.assertEqual(specs[1].manifest, Path("rf.json"))

    def test_config_relative_paths_resolve_to_base(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = {
                "backends": [
                    {
                        "backend": "ultralytics-onnx",
                        "model": "models/a.onnx",
                        "manifest": "models/a.json",
                    }
                ]
            }
            specs = specs_from_config(payload, base_directory=root)
            self.assertEqual(specs[0].model, root / "models" / "a.onnx")


class BakeoffRunTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_run_bakeoff_sequential_mocked_process(self) -> None:
        calls: list[str] = []

        def fake_process(**kwargs: object) -> VideoRunSummary:
            segmenter = kwargs["segmenter"]
            backend = getattr(segmenter, "backend", "unknown")
            calls.append(str(backend))
            if backend == "ultralytics-onnx":
                return _summary(backend=backend, predicted=5, outlined=4, inf_p50=10.0)
            return _summary(backend=backend, predicted=7, outlined=6, inf_p50=20.0)

        def factory(spec: BakeoffBackendSpec) -> MagicMock:
            segmenter = MagicMock()
            segmenter.backend = spec.backend
            return segmenter

        specs = (
            BakeoffBackendSpec(
                backend="ultralytics-onnx",
                model=self.root / "a.onnx",
                manifest=self.root / "a.json",
            ),
            BakeoffBackendSpec(
                backend="rfdetr",
                model=self.root / "b.onnx",
                manifest=self.root / "b.json",
            ),
        )
        out = self.root / "bakeoff.json"
        report = run_bakeoff(
            input_path=self.root / "clip.mp4",
            backends=specs,
            max_frames=30,
            output_path=out,
            process_video_fn=fake_process,
            segmenter_factory=factory,
        )
        self.assertEqual(calls, ["ultralytics-onnx", "rfdetr"])
        self.assertEqual(report["schema_version"], SCHEMA_VERSION)
        self.assertIsNone(report["winner"])
        runs = report["runs"]
        assert isinstance(runs, list)
        self.assertEqual(len(runs), 2)
        self.assertEqual(runs[0]["backend"], "ultralytics-onnx")
        self.assertEqual(runs[0]["instances_predicted"], 5)
        self.assertEqual(runs[1]["instances_outlined"], 6)
        self.assertEqual(runs[1]["inference_ms_p50"], 20.0)
        self.assertTrue(out.is_file())
        on_disk = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(on_disk["schema_version"], 1)
        self.assertIsNone(on_disk["winner"])

    def test_json_schema_finite_and_serializable(self) -> None:
        summary = _summary()
        row = row_from_summary(
            summary,
            backend="ultralytics-onnx",
            model=Path("m.onnx"),
            manifest=Path("m.json"),
        )
        for key in (
            "frames_processed",
            "instances_predicted",
            "instances_outlined",
            "inference_ms_p50",
            "inference_ms_p95",
            "pipeline_ms_p50",
            "pipeline_ms_p95",
            "frame_budget_misses",
        ):
            value = row[key]
            self.assertTrue(isinstance(value, (int, float)))
            self.assertTrue(math.isfinite(float(value)))

        report = {
            "schema_version": SCHEMA_VERSION,
            "input": "clip.mp4",
            "max_frames": 10,
            "max_seconds": None,
            "runs": [row],
            "winner": None,
            "notes": "test",
        }
        out = self.root / "finite.json"
        write_bakeoff_json(report, out)
        # allow_nan=False would have raised on non-finite; re-load must succeed.
        loaded = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(loaded["schema_version"], 1)

    def test_non_finite_summary_fails_closed(self) -> None:
        summary = _summary(inf_p50=float("nan"))
        with self.assertRaisesRegex(BakeoffError, "finite"):
            row_from_summary(
                summary,
                backend="ultralytics-onnx",
                model=Path("m.onnx"),
                manifest=Path("m.json"),
            )

    def test_config_load_and_run(self) -> None:
        config_path = self.root / "bakeoff.json"
        config_path.write_text(
            json.dumps(
                {
                    "backends": [
                        {
                            "backend": "ultralytics-onnx",
                            "model": "a.onnx",
                            "manifest": "a.json",
                        },
                        {
                            "backend": "rfdetr",
                            "model": "b.onnx",
                            "manifest": "b.json",
                        },
                    ],
                    "max_frames": 5,
                }
            ),
            encoding="utf-8",
        )
        payload = load_bakeoff_config(config_path)
        specs = specs_from_config(payload, base_directory=self.root)
        self.assertEqual(len(specs), 2)

        def fake_process(**_kwargs: object) -> VideoRunSummary:
            return _summary()

        def factory(spec: BakeoffBackendSpec) -> MagicMock:
            segmenter = MagicMock()
            segmenter.backend = spec.backend
            return segmenter

        report = run_bakeoff(
            input_path=self.root / "clip.mp4",
            backends=specs,
            max_frames=5,
            output_path=None,
            process_video_fn=fake_process,
            segmenter_factory=factory,
        )
        self.assertEqual(len(report["runs"]), 2)  # type: ignore[arg-type]


class BakeoffCliTests(unittest.TestCase):
    def test_parser_accepts_backends_flags(self) -> None:
        parser = build_parser()
        arguments = parser.parse_args(
            [
                "bakeoff",
                "--input",
                "clip.mp4",
                "--backends",
                "ultralytics-onnx,rfdetr",
                "--model-a",
                "a.onnx",
                "--manifest-a",
                "a.json",
                "--model-b",
                "b.onnx",
                "--manifest-b",
                "b.json",
                "--max-frames",
                "20",
                "--output",
                "artifacts/bakeoff.json",
            ]
        )
        self.assertEqual(arguments.command, "bakeoff")
        self.assertEqual(arguments.backends, "ultralytics-onnx,rfdetr")
        self.assertEqual(arguments.model_a, Path("a.onnx"))
        self.assertEqual(arguments.max_frames, 20)

    def test_parser_accepts_config(self) -> None:
        parser = build_parser()
        arguments = parser.parse_args(
            [
                "bakeoff",
                "--input",
                "clip.mp4",
                "--config",
                "configs/bakeoff.json",
            ]
        )
        self.assertEqual(arguments.config, Path("configs/bakeoff.json"))

    def test_cli_empty_backends_fails_closed(self) -> None:
        stderr = StringIO()
        stdout = StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "bakeoff",
                    "--input",
                    "clip.mp4",
                    "--backends",
                    "",
                    "--model-a",
                    "a.onnx",
                    "--manifest-a",
                    "a.json",
                ]
            )
        self.assertEqual(code, 2)
        self.assertIn("empty", stderr.getvalue().lower())

    def test_cli_requires_config_or_backends(self) -> None:
        stderr = StringIO()
        with redirect_stdout(StringIO()), redirect_stderr(stderr):
            code = main(["bakeoff", "--input", "clip.mp4"])
        self.assertEqual(code, 2)
        self.assertRegex(stderr.getvalue().lower(), r"config|backends")


if __name__ == "__main__":
    unittest.main()
