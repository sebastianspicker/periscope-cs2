# Lens topology decision record (ledger L1)

Fourth implementation track beside the Anti-Cheat lab (simulation), Radar
(memory-side) and Vision (pixel-side capture): a smartphone pointed at the game monitor captures
the game with its camera, runs vision enhancement on-device, and
connects to the PC by cable. Full research provenance:
The original Lens addendum from the 2026-11-05/06 research sessions.

Status: architecture decision record. No live claims are made here; every
runtime number below is a planning budget, measured values land in the L7
harness.

Placement: the lens Python modules live in this `lens/` subtree
([`../src/periscope_lens/`](../src/periscope_lens/)), with the Android
companion under [`../android/`](../android/).
The track was briefly implemented inside `vision/` during the session and
moved out on 2026-11-06 so the repository structure stays anti-cheat,
lens, radar, vision. The package is self-contained (numpy only) and carries
the same AGPL-3.0-only license as vision.

## Topologies

| | A - phone as camera | B - phone computes | C - hybrid |
|---|---|---|---|
| Capture | phone camera → USB → PC (UVC / scrcpy / DroidCam) | phone camera → on-device NPU | both |
| Inference | PC (existing Vision pipeline) | phone (ncnn/QNN/ONNX Runtime Mobile) | both, fused |
| PC load | full | ~zero | medium |
| Phone | interchangeable dumb sensor | capable device required (NPU, Camera2 high-speed, USB 3) | capable |
| Latency budget | 30–80 ms camera chain + PC inference | 40–100 ms total (exposure 8–20 + USB 10–30 + NPU 10–30 + return 5–10 + HID 1–8) | A-like |
| Failure modes | PC pipeline bugs | phone app bugs, HID variance | both |

Decision: build A first, B second, C last. A reuses the existing Vision
pipeline wholesale and de-risks calibration and optics (L3/L4) with the
least new code; B is the user's stated end state and depends on the same L3
calibration plus the L6 transport; C is a research mode once both exist.

Consequence for assist feel: at a 40–100 ms measured budget this is a
tracking/velocity assist, not a flick assist. Compensation (L7) is a
constant-velocity predictor over the *measured* latency; the L9 policy
exposes only subtle defaults.

## Device-class requirements (topology B)

- SoC with a usable NPU path (Snapdragon 8-class or equivalent); the L5
  benchmark table records the actual device, model, input size, fps and
  p50/p95 inference before any claim.
- Camera2 high-speed (60 fps) capture support at the working resolution.
- USB data connection that survives the configured transport: ADB forward
  (no permissions beyond USB debugging), USB-tethering RNDIS, or raw USB
  serial (AOA). Wireless adds 100–200 ms and is out of scope for the assist
  path.
- For the phone-as-HID output path (L8): kernel/configfs USB-gadget support,
  feature-detected per device - never assumed.

## Posture mapping

- Camera capture is posture-clean by construction: the game PC runs
  nothing and shares nothing; the phone watches the same light a human
  watches.
- Actuation paths (L8/L9) are opt-in, mirroring
  `radar/adapters/real/` gating; nothing default-on.
- This track's capture is camera-based today; hardware injectors
  (kmbox-class) are documented as compatible hardware, not implemented.

## Optical reality (why L4 exists)

Pointing a camera at a gaming monitor has three named failure modes
(MIRAGE, arXiv:2602.01559): PWM-dimming flicker-banding under rolling
shutter, moiré between pixel and sensor grids, and tearing/banding under
VRR. Half the mitigation is configuration (fixed exposure matched to
refresh, focus lock, 100% panel brightness, VRR-dim modes off); the other
half is banding-robust preprocessing. L4's preflight gates all of it with
measured checks before anything else is trusted.

## iOS (L10 summary)

Deferred. Windows-side iOS camera access is Continuity (macOS-only) or
proprietary clients; a USB data channel for app-written frames means
usbmuxd plumbing. Android-first; revisit only if topology A via a
proprietary webcam client shows demand.
