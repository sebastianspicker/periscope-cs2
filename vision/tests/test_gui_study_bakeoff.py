"""GUI logic (fakes, no Tk), bakeoff, study and evaluation writer regressions."""

from __future__ import annotations

import argparse
import json
import queue
import threading
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import numpy as np
import pytest

from cs2_vision_access.application.configuration.prefs import OutlinePreferences
from cs2_vision_access.domain.outline import OutlineStyle
from cs2_vision_access.interfaces.cli import style_args
from cs2_vision_access.interfaces.cli.handlers import bakeoff as bakeoff_handler
from cs2_vision_access.interfaces.gui import controller as gui_controller
from cs2_vision_access.interfaces.gui.lifecycle import LifecycleMixin
from cs2_vision_access.interfaces.gui.model import GuiSettings
from cs2_vision_access.interfaces.gui.settings_bind import SettingsBindMixin
from cs2_vision_access.workflows.bakeoff import BakeoffBackendSpec, run_bakeoff
from cs2_vision_access.workflows.evaluation.geometry import (
    PolygonInstance,
    mask_iou,
    match_instances,
)
from cs2_vision_access.workflows.evaluation.masks import write_metrics_json
from cs2_vision_access.workflows.study import (
    StudyError,
    aggregate_ratings,
    parse_rating,
    plan_study_render,
    presentation_order,
    render_study_pack,
    resolve_condition_style,
    write_aggregate_json,
)
from cs2_vision_access.workflows.study.models import (
    TEMPLATE_EXAMPLE_NOTE,
    StudyClip,
    StudyCondition,
    StudyInference,
    StudyPackage,
)

# ---------------------------------------------------------------- bakeoff


def _summary(**_: Any) -> Any:
    from cs2_vision_access.domain.video import VideoRunSummary

    return VideoRunSummary(
        frames_processed=1,
        instances_predicted=0,
        instances_outlined=0,
        stale_predictions_discarded=0,
        degenerate_masks_discarded=0,
        source_fps=30.0,
        source_width=4,
        source_height=4,
        frame_budget_ms=33.0,
        frame_budget_misses=0,
        elapsed_seconds=0.1,
        throughput_fps=10.0,
        inference_ms_p50=1.0,
        inference_ms_p95=1.0,
        pipeline_ms_p50=1.0,
        pipeline_ms_p95=1.0,
        completed_stream=True,
        termination_reason="eof",
        outline_inner_color="#ffffff",
        outline_outer_color="#000000",
        outline_fill_opacity=0.0,
        outline_inner_width_pixels=2,
        outline_outer_width_pixels=4,
        outline_stroke_contrast_ratio=21.0,
        output_path=None,
    )


def test_bakeoff_config_keeps_per_backend_values(tmp_path: Path, monkeypatch: Any) -> None:
    config = {
        "confidence": 0.45,
        "backends": [
            {
                "backend": "ultralytics-onnx",
                "model": "a.onnx",
                "manifest": "a.json",
                "confidence": 0.25,
            },
            {"backend": "rfdetr", "model": "b.onnx", "manifest": "b.json", "confidence": 0.6},
        ],
    }
    config_path = tmp_path / "bakeoff.json"
    config_path.write_text(json.dumps(config), encoding="utf-8")
    seen: list[BakeoffBackendSpec] = []

    def fake_run(**kwargs: Any) -> dict[str, object]:
        def factory(spec: BakeoffBackendSpec) -> Any:
            seen.append(spec)
            return object()

        return run_bakeoff(
            **{**kwargs, "segmenter_factory": factory, "process_video_fn": _summary},
            renderer=object(),  # type: ignore[arg-type]
        ) | {"runs": []}

    monkeypatch.setattr(bakeoff_handler, "run_bakeoff", fake_run)
    monkeypatch.setattr(bakeoff_handler, "_print_json", lambda _r: None)
    arguments = argparse.Namespace(
        config=config_path,
        backends=None,
        max_frames=None,
        max_seconds=None,
        class_name=None,
        confidence=None,
        image_size=None,
        device=None,
        input=tmp_path / "in.mp4",
        output=tmp_path / "out.json",
    )
    bakeoff_handler._handle_bakeoff(arguments)
    assert [spec.confidence for spec in seen] == [0.25, 0.6]


def test_bakeoff_flushes_report_and_records_failure(tmp_path: Path) -> None:
    specs = [
        BakeoffBackendSpec(
            "ultralytics-onnx", tmp_path / "a.onnx", tmp_path / "a.json", confidence=0.25
        ),
        BakeoffBackendSpec("rfdetr", tmp_path / "b.onnx", tmp_path / "b.json", confidence=0.6),
    ]
    output = tmp_path / "report.json"

    def factory(spec: BakeoffBackendSpec) -> Any:
        if spec.backend == "rfdetr":
            raise RuntimeError("boom")
        return object()

    report = run_bakeoff(
        input_path=tmp_path / "in.mp4",
        backends=specs,
        output_path=output,
        segmenter_factory=factory,
        process_video_fn=_summary,
        renderer=object(),  # type: ignore[arg-type]
    )
    written = json.loads(output.read_text(encoding="utf-8"))
    assert written == json.loads(json.dumps(report))
    ok, failed = written["runs"]
    assert ok["confidence"] == 0.25 and ok["termination_reason"] == "eof"
    assert failed["error"] == "RuntimeError: boom" and failed["confidence"] == 0.6


# -------------------------------------------------------------------- GUI


class _Var:
    def __init__(self) -> None:
        self.value: Any = None

    def get(self) -> Any:
        return self.value

    def set(self, value: Any) -> None:
        self.value = value


class _FakeBind(SettingsBindMixin):
    """Auto-creates tk-variable fakes so settings round-trip without Tk."""

    def __init__(self) -> None:
        self.__dict__["_vars"] = {}

    def __getattr__(self, name: str) -> Any:
        return self.__dict__["_vars"].setdefault(name, _Var())

    def _monitor_label(self, index: int) -> str:
        return str(index)

    _device_label = _monitor_label

    def _selected_monitor_index(self) -> int | None:
        return None

    def _selected_device_index(self) -> int | None:
        return None


def test_read_settings_keeps_max_frames_and_outline_kernel() -> None:
    app = _FakeBind()
    app._apply_settings(GuiSettings(max_frames=300, outline_kernel="distance"))
    settings = app._read_settings()
    assert settings.max_frames == 300
    assert settings.outline_kernel == "distance"


class _TclError(Exception):
    pass


class _FakeController:
    def __init__(self, *, running: bool, stops: bool = True) -> None:
        self.running = running
        self.stops = stops
        self.pending_logs: queue.Queue[str] = queue.Queue()
        self.last_summary = None
        self.stop_calls = 0

    @property
    def is_running(self) -> bool:
        return self.running

    def stop(self) -> bool:
        self.stop_calls += 1
        return self.stops


class _FakeApp(LifecycleMixin):
    def __init__(self, controller: _FakeController | None = None) -> None:
        self._controller = controller  # type: ignore[assignment]
        self._tk = SimpleNamespace(TclError=_TclError)
        self._messagebox = SimpleNamespace(showerror=lambda *a: self.errors.append(a))
        self._status_var = _Var()
        self._start_button = SimpleNamespace(config=lambda **kw: self.buttons.update(start=kw))
        self._stop_button = SimpleNamespace(config=lambda **kw: self.buttons.update(stop=kw))
        self._hotkey_buttons: list[Any] = []
        self._preview_queue: queue.Queue[Any] = queue.Queue()
        self._root = SimpleNamespace(after=lambda *a: None)
        self.errors: list[Any] = []
        self.buttons: dict[str, Any] = {}
        self.logs: list[str] = []

    def _log(self, message: str) -> None:
        self.logs.append(message)

    def _show_summary(self, text: str) -> None:
        self.logs.append(text)

    def _read_settings(self) -> Any:
        raise _TclError('expected integer but got ""')


def test_start_live_reports_tcl_error() -> None:
    app = _FakeApp()
    app.start_live()
    assert app.errors and "expected integer" in app.errors[0][1]


def test_still_stopping_keeps_controller_and_finishes_in_poll() -> None:
    controller = _FakeController(running=True, stops=False)
    app = _FakeApp(controller)
    app.stop_live()
    assert app._controller is controller
    assert app._status_var.get() == "Still stopping..."
    app._poll_preview()  # still running: nothing happens
    assert controller.stop_calls == 1
    controller.running = False
    controller.stops = True
    app._poll_preview()
    assert app._controller is None
    assert app._status_var.get() == "Stopped"
    assert app.buttons["start"] == {"state": "normal"}


def test_poll_stops_when_pipeline_ended_and_drains_logs() -> None:
    controller = _FakeController(running=False)
    controller.pending_logs.put("Pipeline error: x")
    app = _FakeApp(controller)
    app._poll_preview()
    assert "Pipeline error: x" in app.logs
    assert app._controller is None
    assert app._status_var.get() == "Stopped"


class _StuckThread:
    def __init__(self) -> None:
        self.alive = True

    def is_alive(self) -> bool:
        return self.alive

    def join(self, timeout: float | None = None) -> None:
        return None


def test_controller_stop_keeps_thread_while_alive() -> None:
    controller = gui_controller.GuiPipelineController(GuiSettings())
    thread = _StuckThread()
    controller._thread = thread  # type: ignore[assignment]
    assert controller.stop() is False
    assert controller._thread is thread
    thread.alive = False
    assert controller.stop() is True
    assert controller._thread is None


def test_worker_error_goes_to_queue_not_on_log(monkeypatch: Any) -> None:
    def boom(**_: Any) -> None:
        raise RuntimeError("worker died")

    monkeypatch.setattr(gui_controller, "build_segmenter", lambda _s: object())
    monkeypatch.setattr(gui_controller, "build_live_pipeline_config", lambda *a: object())
    monkeypatch.setattr(gui_controller, "run_live_pipeline", boom)
    logged_from: list[str] = []
    controller = gui_controller.GuiPipelineController(GuiSettings(overlay=False))
    controller.start(
        on_frame=lambda *a: None,
        on_log=lambda _m: logged_from.append(threading.current_thread().name),
    )
    assert controller._thread is not None
    controller._thread.join(timeout=5)
    assert threading.current_thread().name in set(logged_from)
    assert gui_controller._THREAD_NAME not in logged_from
    assert controller.pending_logs.get_nowait() == "Pipeline error: worker died"


# ------------------------------------------------------------------ study


def _package(tmp_path: Path, clips: list[str], conditions: list[StudyCondition], **kw: Any) -> Any:
    for clip in clips:
        (tmp_path / f"{clip}.mp4").write_bytes(b"x")
    return StudyPackage(
        schema_version=1,
        study_id="s",
        clips=tuple(StudyClip(clip_id=c, input=f"{c}.mp4") for c in clips),
        conditions=tuple(conditions),
        source_path=tmp_path / "study.json",
        **kw,
    )


_BASE = StudyCondition(condition_id="base", kind="baseline")
_OUT = StudyCondition(condition_id="out", kind="outline", outline_preset="maximum-visibility")


def test_stimulus_id_collisions_rejected(tmp_path: Path) -> None:
    clash = _package(
        tmp_path,
        ["a__b", "a"],
        [StudyCondition("c", "baseline"), StudyCondition("b__c", "baseline")],
    )
    with pytest.raises(StudyError, match="collides"):
        plan_study_render(clash, tmp_path / "o", check_inputs=False)
    case = _package(tmp_path, ["A", "a"], [_BASE])
    with pytest.raises(StudyError, match="collides"):
        plan_study_render(case, tmp_path / "o", check_inputs=False)


def test_baseline_gets_same_limits_as_outline(tmp_path: Path) -> None:
    inference = StudyInference(model="m", manifest="m.json", max_frames=7, max_seconds=2.0)
    package = _package(tmp_path, ["a"], [_BASE, _OUT], inference=inference)
    jobs = plan_study_render(package, tmp_path / "o", check_inputs=False)
    assert {(j.max_frames, j.max_seconds) for j in jobs} == {(7, 2.0)}

    calls: list[dict[str, Any]] = []

    def process(**kwargs: Any) -> Any:
        calls.append(kwargs)
        return _summary()

    render_study_pack(
        package,
        tmp_path / "o",
        process_video_fn=process,
        segmenter_factory=lambda *_: object(),  # type: ignore[arg-type,return-value]
        renderer_factory=lambda _style: object(),  # type: ignore[arg-type,return-value]
    )
    assert [c["max_frames"] for c in calls] == [7, 7]
    assert calls[0]["segmenter"].predict(np.zeros((2, 2, 3), np.uint8), frame_index=0) == ()


def test_validate_only_and_overwrite_preserve_existing_files(tmp_path: Path) -> None:
    package = _package(tmp_path, ["a"], [_BASE])
    out = tmp_path / "o"
    render_study_pack(package, out, process_video_fn=_summary)
    manifest = out / "pack-manifest.v1.json"
    before = manifest.read_text(encoding="utf-8")
    render_study_pack(package, out, validate_only=True)
    assert manifest.read_text(encoding="utf-8") == before
    template = out / "ratings.template.jsonl"
    template.write_text("edited\n", encoding="utf-8")
    render_study_pack(package, out, overwrite=True, process_video_fn=_summary)
    assert template.read_text(encoding="utf-8") == "edited\n"


def _rating(participant: str = "P1", condition: str = "c", order: int | None = None, **kw: Any):
    raw: dict[str, Any] = {
        "schema_version": 1,
        "participant_id": participant,
        "session_id": "S",
        "clip_id": "a",
        "condition_id": condition,
        "ratings": {
            "usefulness": 3,
            "clutter": 3,
            "comfort": 3,
            "small_player_visibility": 3,
            "error_confusion": 3,
            "would_enable": True,
        },
        **kw,
    }
    if order is not None:
        raw["presented_order"] = order
    return parse_rating(raw)


def test_aggregate_rejects_duplicates_and_skips_template_row() -> None:
    with pytest.raises(StudyError, match="duplicate"):
        aggregate_ratings([_rating(), _rating()])
    example = _rating(notes=TEMPLATE_EXAMPLE_NOTE)
    assert aggregate_ratings([example, _rating()]).rating_count == 1


def test_aggregate_by_presented_order_and_stable_shuffle() -> None:
    ratings = [_rating("P1", "c1", 0), _rating("P2", "c1", 0), _rating("P1", "c2", 1)]
    aggregate = aggregate_ratings(ratings)
    assert sorted(aggregate.by_presented_order) == ["0", "1"]
    assert "by_presented_order" in aggregate.as_dict()
    assert "by_presented_order" not in aggregate_ratings([_rating()]).as_dict()
    ids = ["x", "y", "z", "w"]
    assert presentation_order("P1", ids) == presentation_order("P1", ids)
    assert sorted(presentation_order("P1", ids)) == sorted(ids)
    # sha256-based: fixed expectation guards against switching to hash()
    assert presentation_order("P1", ids) == sorted(
        ids, key=lambda s: __import__("hashlib").sha256(f"P1\0{s}".encode()).hexdigest()
    )


def test_atomic_writers(tmp_path: Path) -> None:
    out = write_metrics_json({"schema_version": 1}, tmp_path / "m.json")
    assert json.loads(out.read_text(encoding="utf-8")) == {"schema_version": 1}
    aggregate = write_aggregate_json(aggregate_ratings([]), tmp_path / "agg.json")
    assert json.loads(aggregate.read_text(encoding="utf-8"))["rating_count"] == 0
    assert sorted(p.name for p in tmp_path.iterdir()) == ["agg.json", "m.json"]


def _args(**overrides: Any) -> argparse.Namespace:
    values: dict[str, Any] = dict(
        prefs=None,
        outline_preset=None,
        inner_color=None,
        outer_color=None,
        inner_width=None,
        outer_width=None,
        fill_opacity=None,
        fixed_widths=False,
        stroke_pattern=None,
        dash_period=None,
        fill_mode=None,
        halo_blur=None,
        adapt_width=False,
        outline_kernel=None,
    )
    values.update(overrides)
    return argparse.Namespace(**values)


@pytest.mark.parametrize(
    ("cli", "condition"),
    [
        ({}, {}),
        ({"outline_preset": "maximum-visibility"}, {"outline_preset": "maximum-visibility"}),
        (
            {
                "inner_color": "#ffff00",
                "outer_color": "#000000",
                "inner_width": 3,
                "outer_width": 7,
            },
            {
                "inner_color": "#ffff00",
                "outer_color": "#000000",
                "inner_width": 3,
                "outer_width": 7,
            },
        ),
        (
            {"fixed_widths": True, "stroke_pattern": "dashed", "dash_period": 9},
            {"scale_with_frame": False, "stroke_pattern": "dashed", "dash_period_px": 9},
        ),
    ],
)
def test_style_entry_points_agree(
    cli: dict[str, Any], condition: dict[str, Any], tmp_path: Path
) -> None:
    cli_style = style_args._outline_style(_args(**cli))
    condition_style = resolve_condition_style(
        StudyCondition(condition_id="c", kind="outline", **condition),
        base_directory=tmp_path,
    )
    assert isinstance(cli_style, OutlineStyle)
    assert cli_style == condition_style


def test_style_prefs_precedence_agrees(tmp_path: Path) -> None:
    prefs = tmp_path / "prefs.json"
    from cs2_vision_access.application.configuration.prefs import save_outline_preferences

    save_outline_preferences(prefs, OutlinePreferences(preset="maximum-visibility", inner_width=5))
    cli_style = style_args._outline_style(_args(prefs=prefs, outer_width=11))
    condition_style = resolve_condition_style(
        StudyCondition(condition_id="c", kind="outline", prefs="prefs.json", outer_width=11),
        base_directory=tmp_path,
    )
    assert cli_style == condition_style and cli_style.inner_width == 5


# ------------------------------------------------------------- evaluation


def _old_match(gt: Any, preds: Any, *, height: int, width: int, thr: float) -> Any:
    candidates = []
    for gi, g in enumerate(gt):
        for pi, p in enumerate(preds):
            if g.class_id != p.class_id:
                continue
            iou = mask_iou(g.polygon, p.polygon, height=height, width=width)
            if iou > 0.0 and iou >= thr:
                candidates.append((iou, gi, pi))
    candidates.sort(key=lambda item: (-item[0], item[1], item[2]))
    ug: set[int] = set()
    up: set[int] = set()
    matches = []
    for iou, gi, pi in candidates:
        if gi in ug or pi in up:
            continue
        ug.add(gi)
        up.add(pi)
        matches.append((gi, pi, iou))
    matches.sort(key=lambda item: (item[0], item[1]))
    return tuple(matches)


def test_match_instances_matches_reference_implementation() -> None:
    rng = np.random.default_rng(0)

    def poly() -> PolygonInstance:
        x, y = rng.integers(0, 40, 2)
        w, h = rng.integers(8, 30, 2)
        points = ((x, y), (x + w, y), (x + w, y + h), (x, y + h))
        return PolygonInstance(
            class_id=int(rng.integers(0, 2)), polygon=tuple((float(a), float(b)) for a, b in points)
        )

    gt = [poly() for _ in range(6)]
    preds = [poly() for _ in range(7)]
    assert match_instances(gt, preds, height=64, width=64, iou_threshold=0.3) == _old_match(
        gt, preds, height=64, width=64, thr=0.3
    )
