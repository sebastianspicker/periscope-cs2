"""Ledger V6 tests: field-debug overlay determinism and CLI wiring."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

from cs2_vision_access.adapters.capture.live_pipeline.config import LivePipelineConfig
from cs2_vision_access.adapters.rendering.renderer.debug import draw_debug_overlay
from cs2_vision_access.domain.predictions import InstanceMask, Point
from cs2_vision_access.interfaces.cli.handlers.live import register_live_commands


def _mask(box: tuple[float, float, float, float]) -> InstanceMask:
    x0, y0, x1, y1 = box
    polygon: tuple[Point, ...] = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
    return InstanceMask(0, polygon, 0.87, 0, "player")


def test_debug_overlay_is_deterministic() -> None:
    frame = np.zeros((72, 128, 3), dtype=np.uint8)
    frame[:] = (40, 40, 40)
    predictions = [_mask((10, 10, 60, 50)), _mask((70, 20, 120, 60))]
    first = draw_debug_overlay(
        frame.copy(),
        predictions,
        fps=59.7,
        avg_inference_ms=12.3,
        frame_index=42,
    )
    second = draw_debug_overlay(
        frame.copy(),
        predictions,
        fps=59.7,
        avg_inference_ms=12.3,
        frame_index=42,
    )
    assert np.array_equal(first, second), "same inputs must draw same pixels"
    # Clamped inside the frame: nothing out of bounds, boxes actually drawn.
    assert first.shape == frame.shape
    assert (first != 40).any(), "annotations visible on a flat background"


def test_debug_overlay_clamps_out_of_bounds_boxes() -> None:
    frame = np.zeros((60, 80, 3), dtype=np.uint8)
    predictions = [_mask((-50, -50, 500, 500))]
    drawn = draw_debug_overlay(frame, predictions, fps=60.0, avg_inference_ms=8.0, frame_index=0)
    assert drawn is not None
    # A clamped rectangle is degenerate but must not crash or wrap.
    assert drawn.shape == frame.shape


def test_pipeline_config_carries_v6_and_v2_fields() -> None:
    class _StubRecorder:
        def record_indexed(
            self, frame_index: int, instances: object, timestamp_ns: int | None = None
        ) -> bool:
            return True

    config = LivePipelineConfig(debug_viz=True, recorder=_StubRecorder())
    assert config.debug_viz is True
    assert config.recorder is not None
    assert config.recorder.record_indexed(0, []) is True
    assert LivePipelineConfig().debug_viz is False
    assert LivePipelineConfig().recorder is None


def _parse(argv: list[str]) -> argparse.Namespace:
    registry: dict[str, argparse.ArgumentParser] = {}

    class _FakeSubcommands:
        def add_parser(self, name: str, **kwargs: object) -> argparse.ArgumentParser:
            child = argparse.ArgumentParser(prog=name)
            registry[name] = child
            return child

    register_live_commands(_FakeSubcommands())  # type: ignore[arg-type]
    return registry["live"].parse_args(argv)


def test_cli_parses_debug_and_record_flags(tmp_path: Path) -> None:
    record_path = tmp_path / "session.ndjson"
    args = _parse(["--debug-viz", "--record", str(record_path)])
    assert args.debug_viz is True
    assert args.record == record_path

    defaults = _parse([])
    assert not defaults.debug_viz
    assert defaults.record is None
