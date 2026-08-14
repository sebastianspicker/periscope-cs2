"""Shared GUI constants (no tkinter dependency)."""

from __future__ import annotations

PREVIEW_POLL_MS = 50
PREVIEW_QUEUE_MAX = 4
PREVIEW_MAX_WIDTH = 640
CAPTURE_BACKENDS = ("auto", "dshow", "msmf", "avfoundation", "v4l2")
OUTPUT_MODES = ("overlay", "alpha", "green")
