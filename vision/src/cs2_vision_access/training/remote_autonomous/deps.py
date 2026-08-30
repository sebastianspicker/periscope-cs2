"""Patch-compatible alias for remote-autonomous phase dependencies."""

import sys
from importlib import import_module

sys.modules[__name__] = import_module("cs2_vision_access.workflows.training.remote_autonomous.deps")
