# Periscope Lens

Third Periscope track, beside [Radar](../radar/) (memory-side) and
[Vision](../vision/) (pixel-side): a **smartphone pointed at the game
monitor**, capturing the game with its camera, running vision enhancement
on-device, connected to the PC **by cable** to feed aim assistance.

Camera-only by construction: the game PC runs nothing and shares nothing.
Actuation is lab-only opt-in; nothing here is a supported cheat product.

## Status

Pre-alpha research scaffolding. The Python package (`periscope_lens`) ships
the portable policy core — calibration, optical preflight, USB transport,
latency compensation, output shaping, assist policy — fully unit-tested. The
Android companion (`android/`) is a source drop with an honest gaps list and
no CI lane. End-to-end live operation is **not** claimed.

## Layout

| Path | Contents |
|------|----------|
| `src/periscope_lens/` | Portable core: `calibration`, `preflight`, `transport`, `latency`, `output`, `policy`, `phone` (capture source contract) |
| `tests/` | Unit tests for every portable module (no phone, no hardware) |
| `docs/TOPOLOGY.md` | Topology decision record (A/B/C, latency budget, posture mapping) |
| `docs/OUTPUT-SUPPORT.md` | Output-path decision ladder and per-device support matrix |
| `android/` | Kotlin companion sources (Camera2/ORT seams, wire encoder, HID probe) |

## Quick start

```bash
cd lens
uv venv --python 3.12
uv sync --frozen --extra dev
uv run pytest tests/ -q
uv run ruff check src/ tests/ && uv run mypy --strict src/periscope_lens/
```

## Where the boundary sits

The lens modules are deliberately self-contained (numpy only, no imports
from the Vision package): the track inherits nothing at runtime, so it can
be reviewed, licensed and versioned on its own. `tests/` is the evidence;
`docs/TOPOLOGY.md` explains the latency budget that makes this a tracking
assist rather than a flick assist.

## Licensing

AGPL-3.0-only for this subtree (see `LICENSE`). Third-party verdicts and
the one derived stub live in `NOTICE.md`.

## Posture summary

- Capture: camera only — posture-clean, no game-PC access at all.
- Output: HID gadget / PC agent / targets-only, all opt-in.
- Hardware injector kits (kmbox-class): documented as compatible, never
  implemented here.
