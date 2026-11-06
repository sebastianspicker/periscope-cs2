#!/usr/bin/env python3
"""Emit a `periscope.bridge.v1` snapshot from a radar web snapshot (E72).

Input is the radar web service JSON (or a minimal object with `local` and
`players`/`entities`). The output is the versioned contract in
docs/decisions/0003-radar-vision-bridge.md, written atomically.

Usage:
    python radar/scripts/emit_bridge_snapshot.py --input snapshot.json --output bridge.json
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

SCHEMA = "periscope.bridge.v1"


def _entity(raw: dict, index: int) -> dict:
    pos = raw.get("pos") or [raw.get("x", 0.0), raw.get("y", 0.0), raw.get("z", 0.0)]
    if not isinstance(pos, list) or len(pos) < 2:
        raise ValueError(f"entity {index}: bad pos")
    return {
        "id": str(raw.get("id") or raw.get("handle") or f"e:{index}"),
        "name": str(raw.get("name") or ""),
        "team": int(raw.get("team") or 0),
        "x": float(pos[0]),
        "y": float(pos[1]),
        "z": float(pos[2]) if len(pos) > 2 else 0.0,
        "weapon": str(raw.get("weapon_name") or raw.get("weapon") or ""),
        "spotted": bool(raw.get("spotted", True)),
        "los_visible": (
            bool(raw["los_visible"]) if raw.get("los_visible") is not None else None
        ),
    }


def convert(source: dict) -> dict:
    local = source.get("local") or {}
    pos = local.get("pos") or [local.get("x", 0.0), local.get("y", 0.0), local.get("z", 0.0)]
    if not isinstance(pos, list) or len(pos) < 2:
        raise ValueError("local: bad pos")
    raw_entities = source.get("players") or source.get("entities") or []
    if not isinstance(raw_entities, list):
        raise ValueError("players/entities must be an array")
    return {
        "schema": SCHEMA,
        "generated_ms": int(time.time() * 1000),
        "map": str(source.get("map") or source.get("map_name") or ""),
        "local": {
            "x": float(pos[0]),
            "y": float(pos[1]),
            "z": float(pos[2]) if len(pos) > 2 else 0.0,
        },
        "entities": [_entity(raw, index) for index, raw in enumerate(raw_entities)],
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)

    source = json.loads(args.input.read_text())
    if not isinstance(source, dict):
        print("input must be an object", file=sys.stderr)
        return 1
    output = convert(source)
    encoded = json.dumps(output, indent=2, sort_keys=True).encode("utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_bytes(encoded)
    temporary.replace(args.output)
    print(f"wrote {args.output} ({len(output['entities'])} entities)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
