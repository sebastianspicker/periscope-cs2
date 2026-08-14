from __future__ import annotations

import unittest

from cs2_vision_access.inference.pipeline.diagnostics import (
    _percentile as diagnostics_percentile,
)
from cs2_vision_access.video import _percentile
from cs2_vision_access.video.stats import percentile


class VideoStatisticsTests(unittest.TestCase):
    def test_percentiles_are_interpolated_without_halving(self) -> None:
        values = [10.0, 20.0, 30.0]

        self.assertEqual(_percentile(values, 50), 20.0)
        self.assertEqual(_percentile([10.0], 95), 10.0)
        self.assertEqual(_percentile([], 95), 0.0)

    def test_linear_interpolation_between_samples(self) -> None:
        # Nearest-index would return 10.0 for p=25; linear returns 15.0.
        self.assertEqual(percentile([10.0, 20.0, 30.0], 25), 15.0)
        self.assertEqual(percentile([10.0, 20.0, 30.0], 75), 25.0)

    def test_video_and_diagnostics_percentile_agree(self) -> None:
        samples = [
            ([], 95),
            ([7.0], 50),
            ([10.0, 20.0, 30.0], 50),
            ([10.0, 20.0, 30.0], 25),
            ([1.0, 2.0, 3.0, 4.0, 5.0], 90),
        ]
        for values, p in samples:
            with self.subTest(values=values, p=p):
                self.assertEqual(
                    _percentile(values, p),
                    diagnostics_percentile(values, p),
                )
                self.assertEqual(
                    percentile(values, p),
                    diagnostics_percentile(values, p),
                )
