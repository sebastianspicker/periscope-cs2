"""Ledger V4 tests: bounding-box prefilter in ``match_instances``.

The prefilter must be behavior-preserving (bit-identical match output) while
skipping rasterization for bbox-disjoint class-matched pairs, the "seen, not
ranked" audit note about G×P full-frame rasterization, made structural.
"""

from __future__ import annotations

import random
from typing import Any

import pytest

from cs2_vision_access.workflows.evaluation import geometry as geometry_module
from cs2_vision_access.workflows.evaluation.geometry import (
    PolygonInstance,
    match_instances,
)
from cs2_vision_access.workflows.evaluation.raster import rasterize_polygon

Frame = tuple[int, int]


def _polygon(x: float, y: float, size: float) -> tuple[tuple[float, float], ...]:
    return ((x, y), (x + size, y), (x + size, y + size), (x, y + size))


def _brute_force(
    ground_truth: list[PolygonInstance],
    predictions: list[PolygonInstance],
    *,
    height: int,
    width: int,
    iou_threshold: float,
) -> tuple[tuple[int, int, float], ...]:
    """The unfiltered reference algorithm (pre-V4), inlined for parity."""
    candidates: list[tuple[float, int, int]] = []
    gt_masks: dict[int, Any] = {}
    pred_masks: dict[int, Any] = {}

    def mask(cache: dict[int, Any], index: int, instance: PolygonInstance) -> Any:
        if index not in cache:
            cache[index] = rasterize_polygon(instance.polygon, height=height, width=width)
        return cache[index]

    for gt_index, gt in enumerate(ground_truth):
        for pred_index, pred in enumerate(predictions):
            if gt.class_id != pred.class_id:
                continue
            gt_mask = mask(gt_masks, gt_index, gt)
            pred_mask = mask(pred_masks, pred_index, pred)
            union = int((gt_mask | pred_mask).sum())
            iou = float((gt_mask & pred_mask).sum()) / union if union else 0.0
            if iou > 0.0 and iou >= iou_threshold:
                candidates.append((iou, gt_index, pred_index))
    candidates.sort(key=lambda item: (-item[0], item[1], item[2]))
    used_gt: set[int] = set()
    used_pred: set[int] = set()
    matches: list[tuple[int, int, float]] = []
    for iou, gt_index, pred_index in candidates:
        if gt_index in used_gt or pred_index in used_pred:
            continue
        used_gt.add(gt_index)
        used_pred.add(pred_index)
        matches.append((gt_index, pred_index, iou))
    matches.sort(key=lambda item: (item[0], item[1]))
    return tuple(matches)


def test_prefilter_parity_on_random_scenes() -> None:
    rng = random.Random(20261106)
    for _scene in range(20):
        height, width = 64, 64
        truth = [
            PolygonInstance(
                class_id=rng.choice([0, 1]),
                polygon=_polygon(rng.uniform(0, 56), rng.uniform(0, 56), rng.uniform(2, 16)),
            )
            for _ in range(6)
        ]
        predictions = [
            PolygonInstance(
                class_id=rng.choice([0, 1]),
                polygon=_polygon(rng.uniform(-4, 60), rng.uniform(-4, 60), rng.uniform(2, 16)),
            )
            for _ in range(6)
        ]
        got = match_instances(truth, predictions, height=height, width=width)
        expected = _brute_force(truth, predictions, height=height, width=width, iou_threshold=0.5)
        assert got == expected


def test_disjoint_pairs_never_rasterize(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[int] = []
    real = geometry_module.rasterize_polygon

    def counting(*args: Any, **kwargs: Any) -> Any:
        calls.append(1)
        return real(*args, **kwargs)

    monkeypatch.setattr(geometry_module, "rasterize_polygon", counting)

    # Two class-matched pairs on opposite corners: nothing overlaps.
    truth = [PolygonInstance(class_id=0, polygon=_polygon(0, 0, 10))]
    predictions = [PolygonInstance(class_id=0, polygon=_polygon(50, 50, 10))]
    matches = match_instances(truth, predictions, height=64, width=64)
    assert matches == ()
    assert calls == [], "bbox-disjoint pairs must skip rasterization"

    # Overlapping pair: both polygons rasterized (threshold lowered so the
    # interior-fill IoU of the offset squares clears it).
    truth = [PolygonInstance(class_id=0, polygon=_polygon(0, 0, 10))]
    predictions = [PolygonInstance(class_id=0, polygon=_polygon(5, 5, 10))]
    matches = match_instances(truth, predictions, height=64, width=64, iou_threshold=0.1)
    assert len(matches) == 1
    assert len(calls) == 2


def test_touching_boxes_still_score() -> None:
    """Closed-box overlap: a shared edge keeps the pair eligible (the raster
    decides whether IoU > 0), so the prefilter never drops a possible match."""
    truth = [PolygonInstance(class_id=0, polygon=_polygon(0, 0, 10))]
    predictions = [PolygonInstance(class_id=0, polygon=_polygon(10, 0, 10))]
    # Sharing only an edge column: rasterized masks touch but IoU is 0.
    matches = match_instances(truth, predictions, height=64, width=64)
    assert matches == ()

    # One-pixel overlap: positive IoU, must match with a low threshold.
    predictions = [PolygonInstance(class_id=0, polygon=_polygon(9, 0, 10))]
    matches = match_instances(truth, predictions, height=64, width=64, iou_threshold=0.01)
    assert len(matches) == 1 and matches[0][2] > 0.0


def test_class_mismatch_and_thresholds_unchanged() -> None:
    truth = [PolygonInstance(class_id=0, polygon=_polygon(0, 0, 10))]
    predictions = [PolygonInstance(class_id=1, polygon=_polygon(0, 0, 10))]
    assert match_instances(truth, predictions, height=64, width=64) == ()
    with pytest.raises(Exception, match="iou_threshold"):
        match_instances(truth, truth, height=64, width=64, iou_threshold=1.5)


def test_empty_inputs() -> None:
    assert match_instances([], [], height=64, width=64) == ()
    truth = [PolygonInstance(class_id=0, polygon=_polygon(0, 0, 10))]
    assert match_instances(truth, [], height=64, width=64) == ()
    assert match_instances([], truth, height=64, width=64) == ()
