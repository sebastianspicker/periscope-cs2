"""Ledger V7 tests: passive CV-validator agreement harness."""

from __future__ import annotations

import pytest

from cs2_vision_access.application.live.cv_validate import (
    ValidatorError,
    ValidatorReport,
    bbox_iou,
)
from cs2_vision_access.domain.predictions import InstanceMask, Point


def _mask(frame_index: int, box: tuple[float, float, float, float]) -> InstanceMask:
    x0, y0, x1, y1 = box
    polygon: tuple[Point, ...] = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
    return InstanceMask(frame_index, polygon, 0.9, 0, "player")


def test_bbox_iou_basics() -> None:
    unit = ((0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0))
    assert bbox_iou(unit, unit) == 1.0
    assert bbox_iou(unit, ((100.0, 100.0), (110.0, 100.0), (110.0, 110.0), (100.0, 110.0))) == 0.0
    assert bbox_iou(unit, ()) == 0.0
    # Half-overlap squares: intersection 5×5=25, union 100+100−25=175.
    assert bbox_iou(unit, ((5.0, 5.0), (15.0, 5.0), (15.0, 15.0), (5.0, 15.0))) == (
        pytest.approx(25.0 / 175.0)
    )


def test_perfect_agreement_and_empty_frames() -> None:
    report = ValidatorReport()
    frame = report.record_frame(0, [_mask(0, (0, 0, 10, 10))], [_mask(0, (0, 0, 10, 10))])
    assert frame.matched == 1 and frame.missed == 0 and frame.spurious == 0
    assert frame.agreement_rate == 1.0

    # Two empty sets agree perfectly; empty reference with detections is
    # total disagreement (spurious).
    empty = report.record_frame(1, [], [])
    assert empty.agreement_rate == 1.0
    spurious = report.record_frame(2, [], [_mask(2, (0, 0, 5, 5))])
    assert spurious.agreement_rate == 0.0 and spurious.spurious == 1


def test_missed_and_spurious_counting() -> None:
    report = ValidatorReport()
    frame = report.record_frame(
        0,
        [_mask(0, (0, 0, 10, 10)), _mask(0, (50, 50, 60, 60))],
        [_mask(0, (0, 0, 10, 10)), _mask(0, (100, 100, 110, 110))],
    )
    assert frame.matched == 1
    assert frame.missed == 1
    assert frame.spurious == 1
    assert frame.agreement_rate == pytest.approx(0.5)


def test_one_to_one_greedy_matching() -> None:
    """A strong validator box must not steal a weaker reference match twice."""
    report = ValidatorReport()
    frame = report.record_frame(
        0,
        [_mask(0, (0, 0, 10, 10)), _mask(0, (1, 1, 11, 11))],
        [_mask(0, (0, 0, 10, 10))],
    )
    assert frame.matched == 1
    assert frame.missed == 1
    summary = report.summary()
    assert summary["frames"] == 1.0
    assert summary["agreement_rate"] == pytest.approx(0.5)


def test_frame_index_consistency_enforced() -> None:
    report = ValidatorReport()
    with pytest.raises(ValidatorError, match="frame under comparison"):
        report.record_frame(7, [_mask(0, (0, 0, 10, 10))], [_mask(0, (0, 0, 10, 10))])


def test_window_is_bounded() -> None:
    report = ValidatorReport(window=3)
    for index in range(10):
        report.record_frame(index, [_mask(index, (0, 0, 10, 10))], [_mask(index, (0, 0, 10, 10))])
    assert len(report.frames) == 3
    assert report.frames[-1].frame_index == 9
    assert report.summary()["frames"] == 3.0


def test_threshold_validation() -> None:
    report = ValidatorReport()
    with pytest.raises(ValidatorError):
        report.record_frame(0, [], [], iou_threshold=0.0)
    with pytest.raises(ValidatorError):
        report.record_frame(0, [], [], iou_threshold=1.5)


def test_empty_summary_is_zero_not_invented() -> None:
    assert ValidatorReport().summary() == {
        "frames": 0.0,
        "agreement_rate": 0.0,
        "mean_iou": 0.0,
        "missed_per_frame": 0.0,
        "spurious_per_frame": 0.0,
    }
