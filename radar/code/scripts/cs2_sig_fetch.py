"""Fetch/load/transform helpers for CS2 signature updates."""
from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

from cs2_sig_common import (
    CRITICAL_GLOBALS,
    ENTITY_CONSTANTS,
    FIELD_MAP,
    GLOBAL_KEYS,
)

_ALLOWED_SOURCE_HOST = "raw.githubusercontent.com"
_ALLOWED_SOURCE_PATH_PREFIX = "/a2x/cs2-dumper/"


def validate_source_url(url: str) -> None:
    """Require the pinned HTTPS cs2-dumper source before opening a URL."""
    parsed = urlparse(url)
    try:
        port = parsed.port
    except ValueError as exc:
        raise ValueError(f"invalid source URL port: {url!r}") from exc
    if parsed.scheme != "https":
        raise ValueError("CS2 dump source must use HTTPS")
    if parsed.username or parsed.password:
        raise ValueError("CS2 dump source must not include URL credentials")
    if parsed.hostname != _ALLOWED_SOURCE_HOST or port not in (None, 443):
        raise ValueError(
            f"CS2 dump source must be {_ALLOWED_SOURCE_HOST} over HTTPS"
        )
    if not parsed.path.startswith(_ALLOWED_SOURCE_PATH_PREFIX):
        raise ValueError(
            "CS2 dump source must stay within the a2x/cs2-dumper repository path"
        )


class _PinnedSourceRedirectHandler(urllib.request.HTTPRedirectHandler):
    """Reject redirect targets outside the pinned HTTPS source before retrieval."""

    def redirect_request(  # type: ignore[override]
        self,
        req: urllib.request.Request,
        fp: Any,
        code: int,
        msg: str,
        headers: Any,
        newurl: str,
    ) -> urllib.request.Request | None:
        validate_source_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def load_json_bytes(data: bytes) -> dict:
    """Parse UTF-8 JSON bytes into a dict. Raises ValueError on bad input."""
    if not data:
        raise ValueError("empty JSON payload")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError(f"JSON is not valid UTF-8: {exc}") from exc
    try:
        obj = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON: {exc}") from exc
    if not isinstance(obj, dict):
        raise ValueError(f"JSON root must be object, got {type(obj).__name__}")
    return obj


def load_json_path(path: Path) -> dict:
    """Load a JSON object from a filesystem path with actionable errors."""
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc
    try:
        return load_json_bytes(raw)
    except ValueError as exc:
        raise RuntimeError(f"cannot parse {path}: {exc}") from exc


def field_lookup(classes: dict, class_name: str, field_name: str) -> int | None:
    """Look up a schema field offset; preferred class first, then any class."""
    if not isinstance(classes, dict):
        return None
    node = classes.get(class_name) or {}
    if not isinstance(node, dict):
        node = {}
    fields = node.get("fields") or {}
    if isinstance(fields, dict) and field_name in fields:
        return int(fields[field_name])
    for _cname, cnode in classes.items():
        if not isinstance(cnode, dict):
            continue
        f = cnode.get("fields") or {}
        if isinstance(f, dict) and field_name in f:
            return int(f[field_name])
    return None


def build_snapshot(
    offsets: dict,
    client_dll: dict,
    source_url: str,
    *,
    generated_at: str | None = None,
) -> dict:
    """
    Transform raw dumper JSON into the lab snapshot shape.

    Raises RuntimeError if a required schema field or critical global is missing.
    """
    if not isinstance(offsets, dict):
        raise RuntimeError("offsets dump root must be a JSON object")
    if not isinstance(client_dll, dict):
        raise RuntimeError("client_dll dump root must be a JSON object")

    client = offsets.get("client.dll") or offsets.get("client_dll") or {}
    if not isinstance(client, dict) or not client:
        raise RuntimeError(
            "offsets dump missing client.dll / client_dll globals map"
        )

    root = client_dll.get("client.dll") or client_dll
    if not isinstance(root, dict):
        raise RuntimeError("client_dll dump missing client.dll root object")
    classes = root.get("classes") or {}
    if not isinstance(classes, dict) or not classes:
        raise RuntimeError("client_dll dump missing classes map")

    globals_out: dict[str, int] = {}
    missing_globals: list[str] = []
    for key in GLOBAL_KEYS:
        val = client.get(key)
        if val is None:
            missing_globals.append(key)
            globals_out[key] = 0
        else:
            try:
                globals_out[key] = int(val)
            except (TypeError, ValueError) as exc:
                raise RuntimeError(
                    f"global {key} is not an integer: {val!r}"
                ) from exc

    if missing_globals and not getattr(build_snapshot, "_quiet_missing", False):
        print(
            f"WARNING: missing globals in dump: {', '.join(missing_globals)}",
            file=sys.stderr,
        )

    for crit in CRITICAL_GLOBALS:
        if not globals_out.get(crit):
            raise RuntimeError(
                f"Dump missing critical global {crit} "
                f"(required non-zero client.dll RVA)"
            )

    fields_out: dict[str, int] = {}
    for cls, name in FIELD_MAP:
        val = field_lookup(classes, cls, name)
        if val is None:
            raise RuntimeError(f"Missing schema field {cls}.{name}")
        fields_out[name] = int(val)

    ts = generated_at or datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return {
        "source": source_url,
        "module": "client.dll",
        "generated_at_utc": ts,
        "globals": globals_out,
        "fields": fields_out,
        "constants": dict(ENTITY_CONSTANTS),
    }


def diff_snapshots(old: dict | None, new: dict) -> list[str]:
    """
    Diff two snapshots. Returns human-readable change lines for
    globals / fields / constants only (timestamp ignored).
    """
    if not old:
        return ["(no previous snapshot)"]
    if not isinstance(new, dict):
        raise RuntimeError("new snapshot must be a dict")
    changes: list[str] = []
    for section in ("globals", "fields", "constants"):
        a = old.get(section) if isinstance(old.get(section), dict) else {}
        b = new.get(section) if isinstance(new.get(section), dict) else {}
        if not isinstance(a, dict) or not isinstance(b, dict):
            raise RuntimeError(f"snapshot section {section} must be a JSON object")
        keys = sorted(set(a) | set(b))
        for k in keys:
            av, bv = a.get(k), b.get(k)
            if av != bv:
                changes.append(f"{section}.{k}: {_fmt_offset(av)} -> {_fmt_offset(bv)}")
    return changes


def _fmt_offset(v: Any) -> str:
    if isinstance(v, int):
        return f"0x{v:X}"
    if v is None:
        return "(absent)"
    return str(v)

def snapshot_values_equal(a: dict | None, b: dict | None) -> bool:
    """True when globals/fields/constants match (ignores timestamps/source)."""
    if a is None or b is None:
        return a is b
    return not [
        c
        for c in diff_snapshots(a, b)
        if c != "(no previous snapshot)"
    ]

def fetch(url: str, timeout: float = 60.0) -> bytes:
    """Fetch a pinned cs2-dumper JSON URL, rejecting unsafe redirect targets."""
    validate_source_url(url)
    if timeout <= 0:
        raise ValueError("fetch timeout must be positive")
    req = urllib.request.Request(
        url, headers={"User-Agent": "ac-lab-offset-updater/1.0"}
    )
    try:
        opener = urllib.request.build_opener(_PinnedSourceRedirectHandler())
        with opener.open(req, timeout=timeout) as resp:
            data = resp.read()
    except urllib.error.HTTPError as exc:
        raise RuntimeError(
            f"HTTP {exc.code} fetching {url}: {exc.reason}"
        ) from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Failed to fetch {url}: {exc.reason}") from exc
    except TimeoutError as exc:
        raise RuntimeError(f"Timed out fetching {url}") from exc
    except ValueError as exc:
        raise RuntimeError(f"Rejected unsafe CS2 dump URL {url}: {exc}") from exc
    if not data:
        raise RuntimeError(f"Empty response from {url}")
    return data
