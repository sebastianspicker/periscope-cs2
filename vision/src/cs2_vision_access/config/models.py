"""Documented compatibility alias for immutable v1 configuration models.

This module deliberately aliases the canonical module so classes retain their
canonical identity for ``isinstance``, pickling, and mocks.
"""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.application.configuration.models")
