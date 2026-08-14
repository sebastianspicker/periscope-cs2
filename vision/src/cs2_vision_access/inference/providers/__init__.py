"""ONNX Runtime GPU provider configuration, session creation, and device benchmarking.

Re-exports the public API from submodules for backward compatibility::

    from cs2_vision_access.inference.providers import create_ort_session
"""

from cs2_vision_access.inference.providers.benchmark import auto_configure, benchmark_device
from cs2_vision_access.inference.providers.detect import gpu_info, recommend_device
from cs2_vision_access.inference.providers.options import resolve_ort_providers
from cs2_vision_access.inference.providers.session import create_ort_session

__all__ = [
    "auto_configure",
    "benchmark_device",
    "create_ort_session",
    "gpu_info",
    "recommend_device",
    "resolve_ort_providers",
]
