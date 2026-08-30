"""Model-runtime adapters."""

from cs2_vision_access.adapters.models.onnxruntime import create_ort_session

__all__ = ["create_ort_session"]
