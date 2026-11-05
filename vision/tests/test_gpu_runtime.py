"""Tests for the pure-Python parts of the GPU / ONNX Runtime provider code."""

from __future__ import annotations

import re
from types import SimpleNamespace
from typing import Any

import numpy as np
import pytest

from cs2_vision_access.adapters.models.runtime.cuda.decode import decode, decode_cpu
from cs2_vision_access.adapters.models.runtime.cuda.kernels import MARCHING_SQUARES_CU
from cs2_vision_access.adapters.models.runtime.cuda.nms import nms
from cs2_vision_access.adapters.models.runtime.cuda.pipeline_postprocess import _num_predictions
from cs2_vision_access.adapters.models.runtime.providers import benchmark
from cs2_vision_access.adapters.models.segmenters._preprocessing import preprocess_vombit


def _stretch_reference(frame: np.ndarray, size: int) -> np.ndarray:
    """NumPy mirror of ``stretch_kernel`` (kernels.py)."""
    h, w = frame.shape[:2]
    fx = np.clip((np.arange(size) + 0.5) * (w / size) - 0.5, 0, w - 1)
    fy = np.clip((np.arange(size) + 0.5) * (h / size) - 0.5, 0, h - 1)
    x0, y0 = np.floor(fx).astype(int), np.floor(fy).astype(int)
    x1, y1 = np.minimum(x0 + 1, w - 1), np.minimum(y0 + 1, h - 1)
    ax, ay = (fx - x0)[None, :, None], (fy - y0)[:, None, None]
    f = frame.astype(np.float32)
    row0 = f[y0][:, x0] + (f[y0][:, x1] - f[y0][:, x0]) * ax
    row1 = f[y1][:, x0] + (f[y1][:, x1] - f[y1][:, x0]) * ax
    out = np.floor(row0 + (row1 - row0) * ay + 0.5)
    return np.expand_dims(np.transpose(out, (2, 0, 1)), 0).astype(np.float32)


def test_stretch_preprocess_matches_preprocess_vombit() -> None:
    frame = np.random.default_rng(0).integers(0, 256, (1080, 1920, 3), dtype=np.uint8)
    ref = preprocess_vombit(frame)
    got = _stretch_reference(frame, 640)
    assert got.shape == ref.shape == (1, 3, 640, 640)
    # OpenCV uses fixed-point bilinear weights: allow one grey level.
    assert np.abs(got - ref).max() <= 1.0


def test_decode_cpu_maps_model_box_to_frame() -> None:
    raw = np.zeros((1, 6, 3), dtype=np.float32)
    raw[0, :4, 1] = (320, 160, 64, 32)  # cx, cy, w, h in 640x640 model space
    raw[0, 4, 1] = 0.9
    boxes, scores, class_ids = decode_cpu(raw, 2, 1920, 1080, 640)
    np.testing.assert_allclose(boxes[1], [864, 243, 1056, 297], atol=1e-3)
    assert scores[1] == pytest.approx(0.9)
    assert class_ids[1] == 0


@pytest.mark.parametrize("size", [640, 1280])
def test_decode_handles_other_prediction_counts(size: int) -> None:
    n = _num_predictions(SimpleNamespace(get_outputs=lambda: []), "out", size)
    assert n == {640: 8400, 1280: 33600}[size]
    raw = np.random.default_rng(1).random((1, 84, n), dtype=np.float32)
    boxes, scores, class_ids = decode(raw, 80, 1920, 1080, size)
    assert boxes.shape == (n, 4) and scores.shape == class_ids.shape == (n,)


def test_num_predictions_prefers_static_output_shape() -> None:
    out = SimpleNamespace(name="out", shape=[1, 84, 1234])
    assert _num_predictions(SimpleNamespace(get_outputs=lambda: [out]), "out", 640) == 1234


def _marching_cases() -> dict[int, list[tuple[str, ...]]]:
    cases: dict[int, list[tuple[str, ...]]] = {}
    for m in re.finditer(r"case (\d+):(.*?)break;", MARCHING_SQUARES_CU, re.S):
        cases[int(m.group(1))] = [
            tuple(a.strip() for a in seg.split(",")[-4:])
            for seg in re.findall(r"emit_contour_segment\(([^;]*)\);", m.group(2))
        ]
    return cases


@pytest.mark.parametrize("k", [1, 2, 4, 8])
def test_marching_squares_three_corner_cases_mirror_single_corner(k: int) -> None:
    cases = _marching_cases()
    assert len(cases[k]) == 1
    assert cases[15 - k] == cases[k]


def test_nms_chain_keeps_a_and_c() -> None:
    boxes = np.array([[0, 0, 10, 10], [4, 0, 14, 10], [8, 0, 18, 10]], dtype=np.float32)
    scores = np.array([0.9, 0.8, 0.7], dtype=np.float32)
    # IoU(A,B) = IoU(B,C) = 6/14 > 0.3 but IoU(A,C) = 2/18 < 0.3.
    assert nms(boxes, scores, 0.3) == [0, 2]


class _FakeSession:
    def __init__(self, shape: list[Any], type_: str) -> None:
        self.inp = SimpleNamespace(name="x", shape=shape, type=type_)
        self.fed: list[np.ndarray] = []

    def get_inputs(self) -> list[Any]:
        return [self.inp]

    def get_outputs(self) -> list[Any]:
        return [SimpleNamespace(name="y")]

    def run(self, _names: list[str], feed: dict[str, np.ndarray]) -> list[Any]:
        self.fed.append(feed["x"])
        return [None]


@pytest.mark.parametrize(
    ("shape", "type_", "dtype", "expected"),
    [
        (["batch", 3, None, None], "tensor(float16)", np.float16, (1, 3, 640, 640)),
        ([None, 3, 320, 320], "tensor(float)", np.float32, (1, 3, 320, 320)),
    ],
)
def test_benchmark_device_builds_concrete_dummy(
    monkeypatch: pytest.MonkeyPatch,
    shape: list[Any],
    type_: str,
    dtype: Any,
    expected: tuple[int, ...],
) -> None:
    session = _FakeSession(shape, type_)
    monkeypatch.setattr(benchmark, "create_ort_session", lambda *a, **k: session)
    result = benchmark.benchmark_device("m.onnx", n_warmup=1, n_bench=2)
    assert result["device"] == "cpu"
    assert session.fed[0].shape == expected and session.fed[0].dtype == dtype


def test_benchmark_failure_is_logged(
    monkeypatch: pytest.MonkeyPatch, caplog: pytest.LogCaptureFixture
) -> None:
    def boom(*_a: Any, **_k: Any) -> None:
        raise RuntimeError("nope")

    monkeypatch.setattr(benchmark, "benchmark_device", boom)
    assert benchmark._benchmark_candidate("m.onnx", "cuda:0") is None
    assert "cuda:0" in caplog.text
