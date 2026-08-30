"""Documented compatibility alias for dataset value types.

The alias avoids loading a second copy of dataclasses under the former module
name, preserving type identity and pickle behavior.
"""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.dataset.types")
