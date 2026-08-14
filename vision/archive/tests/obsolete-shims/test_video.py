"""Backward-compat shim — tests moved to:
  test_video_statistics.py  — VideoStatisticsTests
  test_video_loop.py        — VideoLoopContractTests
"""
from tests.test_video_statistics import *  # noqa: F401, F403
from tests.test_video_loop import *        # noqa: F401, F403
