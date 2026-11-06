"""Minimal local boundary stub for OpenCV's dynamically generated Python API."""

from typing import Any

def __getattr__(name: str) -> Any: ...
