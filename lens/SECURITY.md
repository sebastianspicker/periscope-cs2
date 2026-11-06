# Security - Periscope Lens

## What this project is

Pre-alpha phone-camera track for studying camera-based assist for
Counter-Strike 2: a smartphone watches the monitor, portable modules compute
targets, motion deltas and output reports. The game PC runs nothing and
shares nothing - capture is light the phone sees.

## What it is not

- Not a supported cheat pack; not a production anti-cheat product.
- Not an input injector by default: the output paths (`output.py`,
  `policy.py`) compute deltas; wiring them to HID/OS input is lab opt-in.
- Not an Android release: `android/` is a source drop, runtime-unverified.

## Posture

| Path | Status |
|------|--------|
| camera capture (`phone.py`) | posture-clean by construction |
| HID gadget / PC agent (`output.py`) | lab-only opt-in, never default |
| targets-only (`output.py` ladder fallback) | always available |
| hardware injectors (kmbox-class) | compatible hardware, not implemented |

## Data handling

- The L6 transport carries compact targets only - never raw camera frames.
- The sanitized recording format (Vision V2) contains no addresses; the
  lens transport contains no identifiers beyond frame ids/timestamps.
- Session/room tokens for any relay are the Vision track's concern; the
  lens emits into the relay's `Targets` shape.

## Reporting

Report repository-side issues (build surprise, docs/code mismatch about
read-only behavior) via the Periscope security channel. “How do I evade X
on live VAC/FACEIT” is out of scope here.
