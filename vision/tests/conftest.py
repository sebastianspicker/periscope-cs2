"""Shared pytest configuration for the cs2-vision-access suite."""

from __future__ import annotations

import pytest


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "gpu: requires a CUDA-capable GPU and GPU extras")
    config.addinivalue_line("markers", "cuda: requires CUDA runtime libraries")
    config.addinivalue_line("markers", "live: requires interactive capture or display hardware")
