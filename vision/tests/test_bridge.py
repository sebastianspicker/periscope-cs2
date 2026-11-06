"""Tests for the radar -> vision bridge loader (E72)."""

from __future__ import annotations

import json

import pytest

from cs2_vision_access.application.live.bridge import (
    BridgeError,
    load_snapshot,
    parse_snapshot,
)


def _raw(**overrides: object) -> bytes:
    data = {
        "schema": "periscope.bridge.v1",
        "generated_ms": 1,
        "map": "de_dust2",
        "local": {"x": 0.0, "y": 0.0, "z": 0.0},
        "entities": [
            {
                "id": "h:1",
                "name": "Alice",
                "team": 2,
                "x": 1.0,
                "y": 2.0,
                "z": 3.0,
                "weapon": "AK-47",
                "spotted": True,
                "los_visible": False,
            }
        ],
    }
    data.update(overrides)
    return json.dumps(data).encode()


def test_parse_valid_snapshot() -> None:
    snapshot = parse_snapshot(_raw())
    assert snapshot.map == "de_dust2"
    assert snapshot.entities[0].name == "Alice"
    assert snapshot.entities[0].los_visible is False


def test_optional_fields_are_nullable() -> None:
    snapshot = parse_snapshot(_raw(entities=[{"id": "h:2", "x": 0, "y": 0, "spotted": True}]))
    assert snapshot.entities[0].name == ""
    assert snapshot.entities[0].los_visible is None


@pytest.mark.parametrize(
    "raw",
    [
        b"",
        b"[]",
        b"{}",
        json.dumps({"schema": "other"}).encode(),
        json.dumps({"schema": "periscope.bridge.v1"}).encode(),
    ],
)
def test_invalid_snapshots_reject(raw: bytes) -> None:
    with pytest.raises(BridgeError):
        parse_snapshot(raw)


def test_bad_entity_fields_reject() -> None:
    with pytest.raises(BridgeError):
        parse_snapshot(_raw(entities=[{"id": "h:1", "x": "no", "y": 0, "spotted": True}]))
    with pytest.raises(BridgeError):
        parse_snapshot(_raw(entities=[{"id": "h:1", "x": 0, "y": 0, "spotted": "yes"}]))
    with pytest.raises(BridgeError):
        parse_snapshot(
            _raw(entities=[{"id": "h:1", "x": 0, "y": 0, "spotted": True, "los_visible": 1}])
        )


def test_entity_cap() -> None:
    many = [{"id": f"h:{i}", "x": 0, "y": 0, "spotted": True} for i in range(257)]
    with pytest.raises(BridgeError):
        parse_snapshot(_raw(entities=many))


def test_load_snapshot(tmp_path) -> None:
    path = tmp_path / "bridge.json"
    path.write_bytes(_raw())
    assert load_snapshot(path).entities[0].id == "h:1"
