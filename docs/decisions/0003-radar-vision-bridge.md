# Decision: radar -> vision bridge is a wire contract, not shared code

Status: accepted 2026-11-06 (unparks the E33 question with a narrow shape)

The radar and vision trees stay independent at runtime (decision 0001). The
bridge is therefore a **file contract plus two small readers**, not a shared
library:

- `radar/scripts/emit_bridge_snapshot.py` converts a radar snapshot into
  `periscope.bridge.v1` JSON.
- `vision/src/cs2_vision_access/application/live/bridge.py` validates and
  loads it.

Neither tree imports the other. Either side can be replaced without touching
the other's build.

## Contract v1

```json
{
  "schema": "periscope.bridge.v1",
  "generated_ms": 1760000000000,
  "map": "de_dust2",
  "local": {"x": 0.0, "y": 0.0, "z": 0.0},
  "entities": [
    {
      "id": "h:1234",
      "name": "Alice",
      "team": 2,
      "x": 100.0, "y": 200.0, "z": 0.0,
      "weapon": "AK-47",
      "spotted": true,
      "los_visible": true
    }
  ]
}
```

Rules:

- `schema` is exact; unknown versions are rejected.
- `entities[].id` is stable across frames (handle, not slot).
- Coordinates are world units. The vision side does **not** project them; a
  later screen-space mapping is a separate concern.
- Missing optional fields are `null`, never invented.
- The file is replaced atomically by the writer.

## Why not a shared library

A shared runtime module would have to choose one license (vision is AGPL,
radar is MIT), one build system, and one release cadence. A JSON contract
costs a parser on each side and keeps both products deployable alone.

## Limits

The bridge does not make vision a memory reader. It labels outlines only when
a radar snapshot is available; with no file, vision behaves exactly as before.

Status: the contract and the vision-side loader exist and are tested. The live
loop does not consume the snapshot yet, so no labels appear without a further
integration step.
