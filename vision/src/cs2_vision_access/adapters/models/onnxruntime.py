"""ONNX Runtime adapter boundary.

The runtime implementation remains lazy and lives behind this adapter so model
implementations never need to reach into the inference provider package.
"""

from __future__ import annotations

from typing import Any


def create_ort_session(*args: Any, **kwargs: Any) -> Any:
    """Create a verified ONNX Runtime session through the provider adapter."""
    from cs2_vision_access.adapters.models.runtime.providers.session import (
        create_ort_session as _create,
    )

    return _create(*args, **kwargs)
