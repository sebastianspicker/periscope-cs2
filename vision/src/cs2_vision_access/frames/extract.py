"""Documented compatibility alias for deterministic frame extraction."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.dataset.frames.extract")
