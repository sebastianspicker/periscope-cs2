"""Tests for config-driven multi-stage `train-auto` orchestration."""

from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from cs2_vision_access.cli import build_parser, main
from cs2_vision_access.training.auto import (
    EXIT_FAIL,
    EXIT_OK,
    run_auto_train,
)
from cs2_vision_access.training.auto.config import config_from_mapping
from tests.train_auto_helpers import (
    _mock_cloud_train,
    _write_flat_dataset,
)


class CliTrainAutoTests(unittest.TestCase):
    def test_parser_registers_train_auto(self) -> None:
        parser = build_parser()
        args = parser.parse_args(["train-auto", "--config", "configs/train-auto.example.json"])
        self.assertEqual(args.command, "train-auto")
        self.assertTrue(callable(args.handler))

    def test_cli_end_to_end_mocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg_path = root / "cfg.json"
            cfg_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "mode": "flat_cloud",
                        "run_id": "cli1",
                        "paths": {"work_root": str(work)},
                        "sources": {"prebuilt_flat_root": str(data)},
                        "train": {
                            "backend": "cloud",
                            "allow_leaky_val": True,
                            "class_names": {"0": "player"},
                        },
                        "eval": {"enabled": False},
                    }
                ),
                encoding="utf-8",
            )
            # Patch via run path used by handler — monkeypatch module attribute.
            import cs2_vision_access.cli.handlers.train_auto as handler_mod
            import cs2_vision_access.training.auto as auto_mod

            original = auto_mod.run_auto_train

            def patched(config, **kwargs):  # type: ignore[no-untyped-def]
                return original(config, train_cloud_fn=_mock_cloud_train, **kwargs)

            auto_mod.run_auto_train = patched  # type: ignore[assignment]
            handler_mod.run_auto_train = patched  # type: ignore[assignment]
            try:
                stdout = io.StringIO()
                stderr = io.StringIO()
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    code = main(["train-auto", "--config", str(cfg_path)])
                self.assertEqual(code, EXIT_OK, msg=stderr.getvalue())
                # Stage helpers may print progress lines before the CLI JSON block.
                text = stdout.getvalue()
                json_start = text.rfind("\n{")
                if json_start < 0:
                    json_start = text.find("{")
                else:
                    json_start += 1
                self.assertGreaterEqual(json_start, 0, msg=text)
                payload = json.loads(text[json_start:])
                self.assertEqual(payload["exit_code"], EXIT_OK)
                self.assertEqual(payload["status"], "completed")
            finally:
                auto_mod.run_auto_train = original  # type: ignore[assignment]
                handler_mod.run_auto_train = original  # type: ignore[assignment]

    def test_cli_missing_config_returns_fail(self) -> None:
        stdout = io.StringIO()
        stderr = io.StringIO()
        with redirect_stdout(stdout), redirect_stderr(stderr):
            code = main(
                [
                    "train-auto",
                    "--config",
                    str(Path(tempfile.gettempdir()) / "missing-train-auto.json"),
                ]
            )
        self.assertEqual(code, EXIT_FAIL)
        self.assertIn("error:", stderr.getvalue().lower())


class EventsJsonlTests(unittest.TestCase):
    def test_run_writes_stage_start_and_end_events(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work = root / "auto"
            data = root / "flat"
            _write_flat_dataset(data)
            cfg = config_from_mapping(
                {
                    "schema_version": 1,
                    "mode": "flat_cloud",
                    "run_id": "events1",
                    "paths": {"work_root": str(work)},
                    "sources": {"prebuilt_flat_root": str(data)},
                    "train": {
                        "backend": "cloud",
                        "allow_leaky_val": True,
                        "class_names": {"0": "player"},
                        "smoke": False,
                    },
                    "export": {"promote_to_run_models": True},
                    "eval": {"enabled": False},
                }
            )
            result = run_auto_train(cfg, train_cloud_fn=_mock_cloud_train)
            self.assertEqual(result.exit_code, EXIT_OK)
            events_path = work / "events1" / "events.jsonl"
            self.assertTrue(events_path.is_file(), "expected events.jsonl under run_dir")
            rows = [
                json.loads(line)
                for line in events_path.read_text(encoding="utf-8").splitlines()
                if line.strip()
            ]
            kinds = [r.get("event") for r in rows]
            self.assertIn("stage_start", kinds)
            self.assertIn("stage_end", kinds)
            self.assertIn("complete", kinds)
            # Every stage that completed should have matching start/end.
            started = {r["stage"] for r in rows if r.get("event") == "stage_start"}
            ended = {r["stage"] for r in rows if r.get("event") == "stage_end"}
            self.assertEqual(started, ended)
            self.assertIn("ingest", started)
            self.assertIn("report", started)
            # Soft path on report artifacts
            report = json.loads((work / "events1" / "report.json").read_text(encoding="utf-8"))
            arts = report.get("artifacts") or {}
            self.assertIn("events_jsonl", arts)
            self.assertTrue(Path(arts["events_jsonl"]).is_file())
            # Timestamps look ISO-ish
            for row in rows:
                self.assertIn("ts", row)
                self.assertTrue(str(row["ts"]).endswith("Z") or "+" in str(row["ts"]))
            end_rows = [r for r in rows if r.get("event") == "stage_end"]
            self.assertTrue(all("duration_s" in r for r in end_rows))
            self.assertTrue(all(r.get("status") == "ok" for r in end_rows))


if __name__ == "__main__":
    unittest.main()
