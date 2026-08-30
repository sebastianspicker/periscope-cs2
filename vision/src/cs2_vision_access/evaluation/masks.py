"""Documented compatibility alias for mask evaluation APIs."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.evaluation.masks")
