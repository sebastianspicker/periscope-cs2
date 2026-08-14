"""Offline sequential backend bakeoff (latency / count comparison).

Runs the file-only ``process_video`` path once per backend on the same local
video and emits a schema-versioned JSON report. Does not invent an
acceptance winner; operators compare held-out CS2 footage themselves.
"""

from __future__ import annotations

from cs2_vision_access.bakeoff.config import (
    DEFAULT_MAX_FRAMES,
    DEFAULT_OUTPUT,
    SCHEMA_VERSION,
    BakeoffBackendSpec,
    BakeoffError,
    load_bakeoff_config,
    parse_backends_csv,
    specs_from_cli_pairs,
    specs_from_config,
)
from cs2_vision_access.bakeoff.runner import (
    row_from_summary,
    run_bakeoff,
    write_bakeoff_json,
)

__all__ = [
    "BakeoffBackendSpec",
    "BakeoffError",
    "DEFAULT_MAX_FRAMES",
    "DEFAULT_OUTPUT",
    "SCHEMA_VERSION",
    "load_bakeoff_config",
    "parse_backends_csv",
    "row_from_summary",
    "run_bakeoff",
    "specs_from_cli_pairs",
    "specs_from_config",
    "write_bakeoff_json",
]
