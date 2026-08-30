"""Best-effort stage event log (``events.jsonl``) for auto-train runs."""

from __future__ import annotations

import json
from datetime import UTC, datetime
from pathlib import Path
from typing import Any


def _utc_ts() -> str:
    """Return an ISO-8601 UTC timestamp ending in ``Z``."""
    return datetime.now(UTC).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def append_event(run_dir: str | Path, event: str, **fields: Any) -> Path | None:
    """Append one JSON line to ``run_dir/events.jsonl``.

    Never raises: logging failures are swallowed so the training pipeline is
    not broken by disk/permission issues. Returns the events path on success,
    or ``None`` on failure.
    """
    try:
        destination = Path(run_dir) / "events.jsonl"
        destination.parent.mkdir(parents=True, exist_ok=True)
        record: dict[str, Any] = {"ts": _utc_ts(), "event": str(event)}
        for key, value in fields.items():
            if value is None:
                continue
            record[key] = value
        with destination.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(record, sort_keys=True, default=str) + "\n")
        return destination
    except Exception:  # noqa: BLE001 — best-effort only
        return None


__all__ = ["append_event"]
