"""Documented compatibility alias for the segmentation protocol."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.adapters.models.segmenters.protocol")
