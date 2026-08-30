"""Documented compatibility alias for training-data contracts."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.training.contracts")
