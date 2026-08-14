from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import MagicMock

from cs2_vision_access.cli import main
from cs2_vision_access.renderer import OutlineStyle, get_outline_preset
from cs2_vision_access.study import (
    RATING_LIKERT_FIELDS,
    RATING_SCHEMA_VERSION,
    SCHEMA_VERSION,
    StudyError,
    aggregate_ratings,
    load_ratings_jsonl,
    load_study_package,
    parse_rating,
    parse_study_package,
    plan_study_render,
    render_study_pack,
    resolve_condition_style,
    stimulus_id_for,
    write_aggregate_json,
)


def _minimal_package(**overrides: object) -> dict[str, object]:
    payload: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "study_id": "pilot-a",
        "clips": [
            {
                "clip_id": "clip-a",
                "input": "clips/a.mp4",
            }
        ],
        "conditions": [
            {
                "condition_id": "baseline",
                "kind": "baseline",
            },
            {
                "condition_id": "high-vis",
                "kind": "outline",
                "outline_preset": "high-visibility",
            },
        ],
        "inference": {
            "model": "model.onnx",
            "manifest": "model.json",
        },
    }
    payload.update(overrides)
    return payload


class StudySchemaTests(unittest.TestCase):
    def test_example_study_document_parses(self) -> None:
        example = Path(__file__).resolve().parents[1] / "docs" / "examples" / "study.v1.json"
        raw = json.loads(example.read_text(encoding="utf-8"))
        package = parse_study_package(raw)
        self.assertEqual(package.schema_version, SCHEMA_VERSION)
        self.assertEqual(package.study_id, "pilot-low-vision-outlines")
        self.assertEqual(len(package.clips), 2)
        self.assertEqual(len(package.conditions), 4)
        self.assertIsNotNone(package.inference)
        assert package.inference is not None
        self.assertEqual(package.inference.max_frames, 1800)
        outline_ids = [
            condition.condition_id
            for condition in package.conditions
            if condition.kind == "outline"
        ]
        self.assertIn("cyan-dashed", outline_ids)

    def test_round_trip_as_json_keys(self) -> None:
        package = parse_study_package(_minimal_package())
        again = parse_study_package(package.as_json())
        self.assertEqual(again.study_id, package.study_id)
        self.assertEqual(len(again.clips), len(package.clips))
        self.assertEqual(len(again.conditions), len(package.conditions))

    def test_unknown_root_key_fails_closed(self) -> None:
        payload = _minimal_package()
        payload["upload_url"] = "https://example.invalid"
        with self.assertRaisesRegex(StudyError, "unknown"):
            parse_study_package(payload)

    def test_missing_required_root_key_fails(self) -> None:
        payload = _minimal_package()
        del payload["clips"]
        with self.assertRaisesRegex(StudyError, "missing"):
            parse_study_package(payload)

    def test_schema_version_must_be_one(self) -> None:
        with self.assertRaisesRegex(StudyError, "schema_version"):
            parse_study_package(_minimal_package(schema_version=2))
        with self.assertRaisesRegex(StudyError, "schema_version"):
            parse_study_package(_minimal_package(schema_version=True))

    def test_duplicate_clip_and_condition_ids_fail(self) -> None:
        with self.assertRaisesRegex(StudyError, "duplicate clip_id"):
            parse_study_package(
                _minimal_package(
                    clips=[
                        {"clip_id": "same", "input": "a.mp4"},
                        {"clip_id": "same", "input": "b.mp4"},
                    ]
                )
            )
        with self.assertRaisesRegex(StudyError, "duplicate condition_id"):
            parse_study_package(
                _minimal_package(
                    conditions=[
                        {"condition_id": "x", "kind": "baseline"},
                        {"condition_id": "x", "kind": "baseline"},
                    ],
                    inference=None,
                )
            )

    def test_path_segment_rules(self) -> None:
        with self.assertRaisesRegex(StudyError, "path separators|path segment"):
            parse_study_package(_minimal_package(study_id="../escape"))
        with self.assertRaisesRegex(StudyError, "path separators|path segment"):
            parse_study_package(
                _minimal_package(
                    clips=[{"clip_id": "a/b", "input": "x.mp4"}],
                )
            )

    def test_outline_requires_inference(self) -> None:
        payload = _minimal_package()
        del payload["inference"]
        with self.assertRaisesRegex(StudyError, "inference is required"):
            parse_study_package(payload)

    def test_baseline_only_allows_missing_inference(self) -> None:
        package = parse_study_package(
            {
                "schema_version": 1,
                "study_id": "baseline-only",
                "clips": [{"clip_id": "c", "input": "c.mp4"}],
                "conditions": [{"condition_id": "b", "kind": "baseline"}],
            }
        )
        self.assertIsNone(package.inference)

    def test_baseline_rejects_outline_fields(self) -> None:
        with self.assertRaisesRegex(StudyError, "baseline must not set"):
            parse_study_package(
                {
                    "schema_version": 1,
                    "study_id": "bad",
                    "clips": [{"clip_id": "c", "input": "c.mp4"}],
                    "conditions": [
                        {
                            "condition_id": "b",
                            "kind": "baseline",
                            "outline_preset": "high-visibility",
                        }
                    ],
                }
            )

    def test_outline_requires_style_source(self) -> None:
        with self.assertRaisesRegex(StudyError, "kind=outline requires"):
            parse_study_package(
                {
                    "schema_version": 1,
                    "study_id": "bad",
                    "clips": [{"clip_id": "c", "input": "c.mp4"}],
                    "conditions": [{"condition_id": "o", "kind": "outline"}],
                    "inference": {"model": "m.onnx", "manifest": "m.json"},
                }
            )

    def test_invalid_outline_preset_fails(self) -> None:
        with self.assertRaisesRegex(StudyError, "outline_preset"):
            parse_study_package(
                _minimal_package(
                    conditions=[
                        {
                            "condition_id": "o",
                            "kind": "outline",
                            "outline_preset": "not-a-preset",
                        }
                    ]
                )
            )

    def test_low_contrast_override_fails_at_parse(self) -> None:
        with self.assertRaisesRegex(StudyError, "style invalid|contrast"):
            parse_study_package(
                _minimal_package(
                    conditions=[
                        {
                            "condition_id": "o",
                            "kind": "outline",
                            "outline_preset": "high-visibility",
                            "inner_color": "#777777",
                            "outer_color": "#777777",
                        }
                    ]
                )
            )

    def test_invalid_stroke_pattern_fails(self) -> None:
        with self.assertRaisesRegex(StudyError, "stroke_pattern"):
            parse_study_package(
                _minimal_package(
                    conditions=[
                        {
                            "condition_id": "o",
                            "kind": "outline",
                            "outline_preset": "high-visibility",
                            "stroke_pattern": "pulse",
                        }
                    ]
                )
            )

    def test_inference_unknown_backend_fails(self) -> None:
        with self.assertRaisesRegex(StudyError, "backend"):
            parse_study_package(
                _minimal_package(
                    inference={
                        "model": "m.onnx",
                        "manifest": "m.json",
                        "backend": "live-capture-backend",
                    }
                )
            )

    def test_inference_confidence_bounds(self) -> None:
        with self.assertRaisesRegex(StudyError, "confidence"):
            parse_study_package(
                _minimal_package(
                    inference={
                        "model": "m.onnx",
                        "manifest": "m.json",
                        "confidence": 1.5,
                    }
                )
            )

    def test_resolve_condition_style_matches_preset(self) -> None:
        package = parse_study_package(_minimal_package())
        condition = next(c for c in package.conditions if c.kind == "outline")
        style = resolve_condition_style(condition, prefs_loader=None)
        self.assertEqual(style, get_outline_preset("high-visibility"))

    def test_resolve_condition_style_override_pattern(self) -> None:
        package = parse_study_package(
            _minimal_package(
                conditions=[
                    {
                        "condition_id": "dashed",
                        "kind": "outline",
                        "outline_preset": "cyan-black",
                        "stroke_pattern": "dashed",
                        "dash_period_px": 20,
                    }
                ]
            )
        )
        style = resolve_condition_style(package.conditions[0], prefs_loader=None)
        self.assertEqual(style.stroke_pattern, "dashed")
        self.assertEqual(style.dash_period_px, 20)
        base = get_outline_preset("cyan-black")
        self.assertEqual(style.inner_color, base.inner_color)


class StudyPlanAndRenderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_plan_expands_clip_times_condition(self) -> None:
        package = parse_study_package(_minimal_package())
        # Attach a fake source path so relative resolution is stable.
        object.__setattr__(package, "source_path", self.root / "study.json")
        clip_path = self.root / "clips" / "a.mp4"
        clip_path.parent.mkdir(parents=True)
        clip_path.write_bytes(b"not-a-real-video")

        # check_inputs=False avoids safety video extension / open checks on dummy bytes
        # when planning pure job geometry; plan still expands the cross product.
        jobs = plan_study_render(package, self.root / "pack", check_inputs=False)
        self.assertEqual(len(jobs), 2)
        self.assertEqual(
            {job.stimulus_id for job in jobs},
            {
                stimulus_id_for("clip-a", "baseline"),
                stimulus_id_for("clip-a", "high-vis"),
            },
        )
        outline_job = next(job for job in jobs if job.kind == "outline")
        self.assertIsInstance(outline_job.style, OutlineStyle)
        baseline_job = next(job for job in jobs if job.kind == "baseline")
        self.assertIsNone(baseline_job.style)

    def test_validate_only_writes_manifest_without_media(self) -> None:
        study_path = self.root / "study.json"
        study_path.write_text(json.dumps(_minimal_package()), encoding="utf-8")
        package = load_study_package(study_path)
        out = self.root / "pack"
        manifest = render_study_pack(package, out, validate_only=True)
        self.assertTrue(manifest["validate_only"])
        self.assertEqual(manifest["job_count"], 2)
        self.assertTrue((out / "pack-manifest.v1.json").is_file())
        stimuli = out / "stimuli"
        self.assertFalse(stimuli.exists() and any(stimuli.iterdir()))

    def test_render_baseline_copies_and_outline_uses_process_hook(self) -> None:
        clips_dir = self.root / "clips"
        clips_dir.mkdir()
        source = clips_dir / "a.mp4"
        source.write_bytes(b"\x00" * 64)

        study_path = self.root / "study.json"
        study_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "study_id": "copy-test",
                    "clips": [{"clip_id": "clip-a", "input": "clips/a.mp4"}],
                    "conditions": [
                        {"condition_id": "baseline", "kind": "baseline"},
                        {
                            "condition_id": "high-vis",
                            "kind": "outline",
                            "outline_preset": "high-visibility",
                        },
                    ],
                    "inference": {
                        "model": "model.onnx",
                        "manifest": "model.json",
                        "max_frames": 10,
                    },
                }
            ),
            encoding="utf-8",
        )
        # Dummy model files so segmenter factory is not invoked when process is mocked
        # after segmenter creation — inject factories that avoid real ONNX.
        (self.root / "model.onnx").write_bytes(b"onnx")
        (self.root / "model.json").write_text("{}", encoding="utf-8")

        package = load_study_package(study_path)
        out = self.root / "pack"

        summary = MagicMock()
        summary.frames_processed = 3
        summary.termination_reason = "end_of_stream"
        summary.instances_outlined = 1

        def fake_process(**kwargs: object) -> MagicMock:
            output_path = Path(str(kwargs["output_path"]))
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(b"rendered")
            return summary

        fake_segmenter = object()

        manifest = render_study_pack(
            package,
            out,
            overwrite=True,
            segmenter_factory=lambda inference, base: fake_segmenter,  # type: ignore[arg-type,return-value]
            process_video_fn=fake_process,  # type: ignore[arg-type]
        )
        baseline_mp4 = out / "stimuli" / "clip-a__baseline.mp4"
        outline_mp4 = out / "stimuli" / "clip-a__high-vis.mp4"
        self.assertTrue(baseline_mp4.is_file())
        self.assertEqual(baseline_mp4.read_bytes(), source.read_bytes())
        self.assertTrue(outline_mp4.is_file())
        self.assertEqual(outline_mp4.read_bytes(), b"rendered")
        self.assertTrue((out / "ratings.template.jsonl").is_file())
        statuses = {job["stimulus_id"]: job["status"] for job in manifest["jobs"]}  # type: ignore[index]
        self.assertEqual(statuses["clip-a__baseline"], "copied")
        self.assertEqual(statuses["clip-a__high-vis"], "rendered")


class StudyRatingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _rating(
        self,
        *,
        participant: str = "P001",
        condition: str = "high-vis",
        usefulness: int = 4,
        would_enable: bool = True,
    ) -> dict[str, object]:
        return {
            "schema_version": RATING_SCHEMA_VERSION,
            "participant_id": participant,
            "session_id": "S001",
            "clip_id": "clip-a",
            "condition_id": condition,
            "ratings": {
                "usefulness": usefulness,
                "clutter": 2,
                "comfort": 4,
                "small_player_visibility": 3,
                "error_confusion": 2,
                "would_enable": would_enable,
            },
        }

    def test_parse_rating_happy_path(self) -> None:
        rating = parse_rating(self._rating())
        self.assertEqual(rating.ratings["usefulness"], 4)
        self.assertTrue(rating.ratings["would_enable"])
        for field_name in RATING_LIKERT_FIELDS:
            self.assertIn(field_name, rating.ratings)

    def test_unknown_rating_key_fails(self) -> None:
        payload = self._rating()
        payload["cloud_sync"] = True
        with self.assertRaisesRegex(StudyError, "unknown"):
            parse_rating(payload)

    def test_likert_out_of_range_fails(self) -> None:
        payload = self._rating(usefulness=6)
        with self.assertRaisesRegex(StudyError, "usefulness"):
            parse_rating(payload)

    def test_would_enable_must_be_bool(self) -> None:
        payload = self._rating()
        payload["ratings"] = {**payload["ratings"], "would_enable": "yes"}  # type: ignore[index]
        with self.assertRaisesRegex(StudyError, "would_enable"):
            parse_rating(payload)

    def test_missing_protocol_field_fails(self) -> None:
        payload = self._rating()
        ratings = dict(payload["ratings"])  # type: ignore[arg-type]
        del ratings["clutter"]
        payload["ratings"] = ratings
        with self.assertRaisesRegex(StudyError, "missing"):
            parse_rating(payload)

    def test_load_jsonl_and_aggregate(self) -> None:
        path = self.root / "ratings.jsonl"
        lines = [
            self._rating(
                participant="P001", condition="baseline", usefulness=2, would_enable=False
            ),
            self._rating(participant="P001", condition="high-vis", usefulness=5, would_enable=True),
            self._rating(participant="P002", condition="high-vis", usefulness=4, would_enable=True),
        ]
        path.write_text(
            "\n".join(json.dumps(line) for line in lines) + "\n",
            encoding="utf-8",
        )
        ratings = load_ratings_jsonl(path)
        self.assertEqual(len(ratings), 3)
        aggregate = aggregate_ratings(ratings)
        self.assertEqual(aggregate.rating_count, 3)
        self.assertEqual(aggregate.participant_count, 2)
        self.assertAlmostEqual(aggregate.mean_likert["usefulness"], (2 + 5 + 4) / 3)
        self.assertAlmostEqual(aggregate.would_enable_rate, 2 / 3)
        high = aggregate.by_condition["high-vis"]
        self.assertEqual(high["rating_count"], 2)
        self.assertAlmostEqual(high["would_enable_rate"], 1.0)  # type: ignore[arg-type]

        out = self.root / "agg.json"
        write_aggregate_json(aggregate, out)
        loaded = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(loaded["rating_count"], 3)

    def test_aggregate_empty(self) -> None:
        aggregate = aggregate_ratings(())
        self.assertEqual(aggregate.rating_count, 0)
        self.assertEqual(aggregate.would_enable_rate, 0.0)


class StudyCliTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_study_render_validate_only_cli(self) -> None:
        study_path = self.root / "study.json"
        study_path.write_text(json.dumps(_minimal_package()), encoding="utf-8")
        out = self.root / "pack"
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            status = main(
                [
                    "study-render",
                    "--study",
                    str(study_path),
                    "--output-directory",
                    str(out),
                    "--validate-only",
                ]
            )
        self.assertEqual(status, 0)
        payload = json.loads(buffer.getvalue())
        self.assertTrue(payload["validate_only"])
        self.assertEqual(payload["job_count"], 2)
        self.assertTrue((out / "pack-manifest.v1.json").is_file())

    def test_study_render_invalid_schema_returns_two(self) -> None:
        study_path = self.root / "bad.json"
        study_path.write_text(
            json.dumps({"schema_version": 1, "study_id": "x"}),
            encoding="utf-8",
        )
        status = main(
            [
                "study-render",
                "--study",
                str(study_path),
                "--output-directory",
                str(self.root / "pack"),
                "--validate-only",
            ]
        )
        self.assertEqual(status, 2)

    def test_study_aggregate_cli(self) -> None:
        ratings_path = self.root / "ratings.jsonl"
        rating = {
            "schema_version": 1,
            "participant_id": "P001",
            "session_id": "S001",
            "clip_id": "clip-a",
            "condition_id": "high-vis",
            "ratings": {
                "usefulness": 5,
                "clutter": 1,
                "comfort": 5,
                "small_player_visibility": 4,
                "error_confusion": 1,
                "would_enable": True,
            },
        }
        ratings_path.write_text(json.dumps(rating) + "\n", encoding="utf-8")
        out = self.root / "agg.json"
        buffer = io.StringIO()
        with redirect_stdout(buffer):
            status = main(
                [
                    "study-aggregate",
                    "--ratings",
                    str(ratings_path),
                    "--output",
                    str(out),
                ]
            )
        self.assertEqual(status, 0)
        payload = json.loads(buffer.getvalue())
        self.assertEqual(payload["rating_count"], 1)
        self.assertTrue(out.is_file())


if __name__ == "__main__":
    unittest.main()
