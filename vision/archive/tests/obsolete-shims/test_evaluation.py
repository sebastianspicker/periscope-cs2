"""Backward-compat shim — test classes live in domain-specific files."""

from __future__ import annotations

import unittest

from tests.test_evaluation_cli import (  # noqa: F401
    EvalMasksCliTests,
    ExtendedEvalCliTests,
    _write_solid_png,
)
from tests.test_evaluation_metrics import (  # noqa: F401
    ComfortMetricTests,
    EvaluationMetricTests,
    NegativesMetricTests,
    TemporalMetricTests,
)
from tests.test_evaluation_yolo import (  # noqa: F401
    FIXTURES,
    YoloLoadAndFixtureTests,
)

if __name__ == "__main__":
    unittest.main()
