"""Exception-policy regression tests for optional CUDA acceleration."""

from __future__ import annotations

import importlib
from types import SimpleNamespace
from typing import Any

import numpy as np
import pytest

from cs2_vision_access.segmenters import _preprocessing
from cs2_vision_access.segmenters.cs2_sam import Cs2SamSegmenter

decode_module = importlib.import_module("cs2_vision_access.inference.cuda.decode")
nms_module = importlib.import_module("cs2_vision_access.inference.cuda.nms")
pipeline_module = importlib.import_module("cs2_vision_access.inference.cuda.pipeline")
postprocess_module = importlib.import_module(
    "cs2_vision_access.inference.cuda.pipeline_postprocess"
)
preprocess_module = importlib.import_module("cs2_vision_access.inference.cuda.preprocess")
cuda_utils = importlib.import_module("cs2_vision_access.inference.cuda.utils")


BACKEND_ERROR_CASES = (
    (ImportError, True),
    (RuntimeError, True),
    (KeyError, False),
    (TypeError, False),
)
RUNTIME_ERROR_CASES = (
    (RuntimeError, True),
    (KeyError, False),
    (TypeError, False),
)


def _raise(error_type: type[Exception]) -> Any:
    raise error_type("test backend failure")


def _record_cpu_fallback(
    calls: list[bool], fallback: tuple[tuple[float, float], ...]
) -> tuple[tuple[float, float], ...]:
    calls.append(True)
    return fallback


@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_mask_polygon_gpu_failure_policy(
    monkeypatch: pytest.MonkeyPatch, error_type: type[Exception], falls_back: bool
) -> None:
    fallback = ((1.0, 1.0), (2.0, 2.0), (3.0, 3.0))
    cpu_calls: list[bool] = []
    monkeypatch.setattr(cuda_utils, "cuda_available", lambda: True)
    monkeypatch.setattr(
        preprocess_module, "gpu_mask_to_polygon", lambda *_args, **_kwargs: _raise(error_type)
    )
    monkeypatch.setattr(
        _preprocessing,
        "_mask_to_polygon_cpu",
        lambda *_args, **_kwargs: _record_cpu_fallback(cpu_calls, fallback),
    )

    if falls_back:
        assert _preprocessing.mask_to_polygon(np.ones((3, 3), dtype=np.uint8)) == fallback
        assert cpu_calls == [True]
    else:
        with pytest.raises(error_type, match="test backend failure"):
            _preprocessing.mask_to_polygon(np.ones((3, 3), dtype=np.uint8))
        assert cpu_calls == []


class _RaisingGpuPipeline:
    def __init__(self, error_type: type[Exception]) -> None:
        self._error_type = error_type

    def run(self, _frame: np.ndarray) -> Any:
        return _raise(self._error_type)


class _Detector:
    def __init__(self) -> None:
        self.calls = 0

    def run(self, _output_names: list[str], _inputs: dict[str, np.ndarray]) -> list[np.ndarray]:
        self.calls += 1
        raw = np.zeros((5, 1), dtype=np.float32)
        raw[:4, 0] = (320.0, 320.0, 64.0, 64.0)
        raw[4, 0] = 0.9
        return [raw]


def _segmenter_with_gpu(error_type: type[Exception]) -> tuple[Cs2SamSegmenter, _Detector]:
    segmenter: Any = object.__new__(Cs2SamSegmenter)
    detector = _Detector()
    segmenter._use_gpu_pipeline = True
    segmenter._gpu_pipeline = _RaisingGpuPipeline(error_type)
    segmenter._detector = detector
    segmenter._det_input_name = "images"
    segmenter._det_output_name = "output0"
    segmenter._det_num_classes = 1
    segmenter.confidence = 0.4
    segmenter.allowed_ids = {0}
    segmenter.manifest = SimpleNamespace(classes={0: "ct"})
    return segmenter, detector


@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_segmenter_gpu_run_failure_policy(
    monkeypatch: pytest.MonkeyPatch, error_type: type[Exception], falls_back: bool
) -> None:
    segmenter, detector = _segmenter_with_gpu(error_type)
    monkeypatch.setattr(_preprocessing, "preprocess_vombit", lambda _frame: np.zeros((1, 3, 1, 1)))
    monkeypatch.setattr(
        importlib.import_module("cs2_vision_access.segmenters.cs2_sam"),
        "preprocess_vombit",
        lambda _frame: np.zeros((1, 3, 1, 1)),
    )

    if falls_back:
        assert segmenter._detect(np.zeros((4, 4, 3), dtype=np.uint8), 4, 4)
        assert detector.calls == 1
    else:
        with pytest.raises(error_type, match="test backend failure"):
            segmenter._detect(np.zeros((4, 4, 3), dtype=np.uint8), 4, 4)
        assert detector.calls == 0


@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_segmenter_gpu_setup_failure_policy(
    monkeypatch: pytest.MonkeyPatch, error_type: type[Exception], falls_back: bool
) -> None:
    segmenter: Any = object.__new__(Cs2SamSegmenter)
    segmenter.device = "cuda:0"
    segmenter._detector = object()
    segmenter._det_num_classes = 1
    segmenter.confidence = 0.4
    segmenter._det_input_name = "images"
    segmenter._det_output_name = "output0"
    segmenter._gpu_pipeline = object()
    segmenter._use_gpu_pipeline = True
    monkeypatch.setattr(cuda_utils, "cuda_available", lambda: True)
    monkeypatch.setattr(
        pipeline_module, "GpuInferencePipeline", lambda *_args, **_kwargs: _raise(error_type)
    )

    if falls_back:
        segmenter._try_enable_gpu_pipeline()
        assert segmenter._gpu_pipeline is None
    else:
        with pytest.raises(error_type, match="test backend failure"):
            segmenter._try_enable_gpu_pipeline()


class _CudaArray(np.ndarray):
    @property
    def __cuda_array_interface__(self) -> dict[str, object]:
        return {}


@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_decode_gpu_failure_policy(
    monkeypatch: pytest.MonkeyPatch, error_type: type[Exception], falls_back: bool
) -> None:
    raw = np.zeros((5, 1), dtype=np.float32).view(_CudaArray)
    raw[:4, 0] = (320.0, 320.0, 64.0, 64.0)
    raw[4, 0] = 0.9
    monkeypatch.setattr(decode_module, "cuda_available", lambda: True)
    monkeypatch.setattr(decode_module, "decode_gpu", lambda *_args: _raise(error_type))

    if falls_back:
        boxes, scores, class_ids = decode_module.decode(raw, 1, 640, 640)
        assert boxes.shape == (1, 4)
        assert scores.shape == (1,)
        assert class_ids.shape == (1,)
    else:
        with pytest.raises(error_type, match="test backend failure"):
            decode_module.decode(raw, 1, 640, 640)


@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_nms_gpu_failure_policy(
    monkeypatch: pytest.MonkeyPatch, error_type: type[Exception], falls_back: bool
) -> None:
    boxes = np.array([[0, 0, 10, 10]], dtype=np.float32)
    scores = np.array([0.9], dtype=np.float32)
    monkeypatch.setattr(nms_module, "cuda_available", lambda: True)
    monkeypatch.setattr(nms_module, "_nms_gpu", lambda *_args: _raise(error_type))

    if falls_back:
        assert nms_module.nms(boxes, scores) == [0]
    else:
        with pytest.raises(error_type, match="test backend failure"):
            nms_module.nms(boxes, scores)


@pytest.mark.parametrize(
    ("function_name", "args"),
    (("gpu_letterbox", (np.zeros((2, 2, 3)), 2)), ("gpu_yolo_decode", (np.zeros((5, 1)), 1, 2, 2))),
)
@pytest.mark.parametrize(("error_type", "falls_back"), BACKEND_ERROR_CASES)
def test_cuda_preprocess_failure_policy(
    monkeypatch: pytest.MonkeyPatch,
    function_name: str,
    args: tuple[object, ...],
    error_type: type[Exception],
    falls_back: bool,
) -> None:
    gpu_name = "_letterbox_gpu" if function_name == "gpu_letterbox" else "_yolo_decode_gpu"
    cpu_name = "_letterbox_cpu" if function_name == "gpu_letterbox" else "_yolo_decode_cpu"
    fallback = ("cpu",) if function_name == "gpu_letterbox" else ("boxes", "scores", "ids")
    monkeypatch.setattr(preprocess_module, "cuda_available", lambda: True)
    monkeypatch.setattr(preprocess_module, gpu_name, lambda *_args: _raise(error_type))
    monkeypatch.setattr(preprocess_module, cpu_name, lambda *_args: fallback)

    function = getattr(preprocess_module, function_name)
    if falls_back:
        assert function(*args) == fallback
    else:
        with pytest.raises(error_type, match="test backend failure"):
            function(*args)


@pytest.mark.parametrize(("error_type", "falls_back"), RUNTIME_ERROR_CASES)
def test_session_dtype_failure_policy(error_type: type[Exception], falls_back: bool) -> None:
    session = SimpleNamespace(get_inputs=lambda: _raise(error_type))

    if falls_back:
        assert postprocess_module._session_input_dtype(session) == np.dtype(np.float32)
    else:
        with pytest.raises(error_type, match="test backend failure"):
            postprocess_module._session_input_dtype(session)


class _FailingBoxes:
    def __init__(self, error_type: type[Exception]) -> None:
        self._error_type = error_type

    def __len__(self) -> int:
        return 1

    def __getitem__(self, _key: object) -> Any:
        return _raise(self._error_type)

    def __array__(self, dtype: np.dtype | None = None, copy: bool | None = None) -> np.ndarray:
        result = np.array([[0.0, 0.0, 1.0, 1.0]], dtype=dtype)
        return result.copy() if copy else result


@pytest.mark.parametrize(("error_type", "falls_back"), RUNTIME_ERROR_CASES)
def test_confidence_filter_failure_policy(error_type: type[Exception], falls_back: bool) -> None:
    boxes = _FailingBoxes(error_type)
    scores = np.array([0.9], dtype=np.float32)
    class_ids = np.array([0], dtype=np.int32)

    if falls_back:
        filtered_boxes, filtered_scores, filtered_class_ids = (
            postprocess_module._filter_by_confidence(boxes, scores, class_ids, 0.5)
        )
        assert isinstance(filtered_boxes, np.ndarray)
        np.testing.assert_array_equal(filtered_scores, scores)
        np.testing.assert_array_equal(filtered_class_ids, class_ids)
    else:
        with pytest.raises(error_type, match="test backend failure"):
            postprocess_module._filter_by_confidence(boxes, scores, class_ids, 0.5)
