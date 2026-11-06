"""Read-only loader for the radar -> vision bridge contract (E72).

The contract is `periscope.bridge.v1` (docs/decisions/0003). This module
validates it strictly and returns immutable records; it never imports radar
code. With no snapshot the caller keeps its normal outline behaviour.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

SCHEMA = "periscope.bridge.v1"
MAX_ENTITIES = 256
MAX_BYTES = 256 * 1024


class BridgeError(ValueError):
    """Malformed or unsupported bridge snapshot."""


@dataclass(frozen=True)
class BridgeEntity:
    id: str
    name: str
    team: int
    x: float
    y: float
    z: float
    weapon: str
    spotted: bool
    los_visible: bool | None


@dataclass(frozen=True)
class BridgeSnapshot:
    generated_ms: int
    map: str
    local_x: float
    local_y: float
    local_z: float
    entities: tuple[BridgeEntity, ...]


def _finite(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise BridgeError(f"{field} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise BridgeError(f"{field} must be finite")
    return number


def _string(value: object, field: str) -> str:
    if not isinstance(value, str):
        raise BridgeError(f"{field} must be a string")
    return value


def parse_snapshot(raw: bytes) -> BridgeSnapshot:
    if len(raw) > MAX_BYTES:
        raise BridgeError(f"snapshot over {MAX_BYTES} bytes")
    try:
        data = json.loads(raw)
    except (ValueError, UnicodeDecodeError) as error:
        raise BridgeError(f"invalid JSON: {error}") from error
    if not isinstance(data, dict):
        raise BridgeError("snapshot must be an object")
    if data.get("schema") != SCHEMA:
        raise BridgeError(f"unsupported schema {data.get('schema')!r}")

    local = data.get("local")
    if not isinstance(local, dict):
        raise BridgeError("local must be an object")
    entities_raw = data.get("entities")
    if not isinstance(entities_raw, list):
        raise BridgeError("entities must be an array")
    if len(entities_raw) > MAX_ENTITIES:
        raise BridgeError(f"over {MAX_ENTITIES} entities")

    entities: list[BridgeEntity] = []
    for index, item in enumerate(entities_raw):
        if not isinstance(item, dict):
            raise BridgeError(f"entities[{index}] must be an object")
        los = item.get("los_visible")
        if los is not None and not isinstance(los, bool):
            raise BridgeError(f"entities[{index}].los_visible must be bool/null")
        spotted = item.get("spotted")
        if not isinstance(spotted, bool):
            raise BridgeError(f"entities[{index}].spotted must be bool")
        entities.append(
            BridgeEntity(
                id=_string(item.get("id"), f"entities[{index}].id"),
                name=_string(item.get("name", ""), f"entities[{index}].name"),
                team=int(_finite(item.get("team", 0), f"entities[{index}].team")),
                x=_finite(item.get("x"), f"entities[{index}].x"),
                y=_finite(item.get("y"), f"entities[{index}].y"),
                z=_finite(item.get("z", 0.0), f"entities[{index}].z"),
                weapon=_string(item.get("weapon", ""), f"entities[{index}].weapon"),
                spotted=spotted,
                los_visible=los,
            )
        )

    generated = data.get("generated_ms", 0)
    if isinstance(generated, bool) or not isinstance(generated, int):
        raise BridgeError("generated_ms must be an integer")
    return BridgeSnapshot(
        generated_ms=generated,
        map=_string(data.get("map", ""), "map"),
        local_x=_finite(local.get("x", 0.0), "local.x"),
        local_y=_finite(local.get("y", 0.0), "local.y"),
        local_z=_finite(local.get("z", 0.0), "local.z"),
        entities=tuple(entities),
    )


def load_snapshot(path: str | Path) -> BridgeSnapshot:
    return parse_snapshot(Path(path).read_bytes())
