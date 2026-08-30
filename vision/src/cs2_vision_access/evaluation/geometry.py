"""Documented compatibility alias for evaluation geometry APIs."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.evaluation.geometry")
