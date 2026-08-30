"""Pure contracts and value objects for the vision application.

This package deliberately has no capture, GUI, model-runtime, or filesystem
side effects.  The legacy modules remain import-compatible facades while new
application code depends on these contracts.
"""

from cs2_vision_access.domain.predictions import InstanceMask, Point
from cs2_vision_access.domain.statistics import percentile

__all__ = ["InstanceMask", "Point", "percentile"]
