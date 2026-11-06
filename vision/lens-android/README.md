# Lens Android companion (ledger L5)

The topology-B phone app: Camera2 capture → ROI inference → compact target
stream over USB (L6 protocol) → optional HID-gadget output (L8). **These
sources are a best-effort implementation drop, not a CI-gated artifact**:
no Android SDK/NDPU toolchain exists in this repository's verification
environment, so everything here is runtime-unverified by house style, the
same posture as the Windows real-capability compile job (dispatch-only CI).

## Layout

```
app/src/main/java/dev/periscope/lens/
  MainActivity.kt      Camera2 loop, ROI discipline, ORT session, USB sender
  TargetsEncoder.kt    L6 wire protocol encoder (pure Kotlin, mirrors
                       vision/src/.../application/lens/transport.py)
  HidGadgetHelper.kt   USB-gadget feature detection + report writing (L8 path 1)
app/build.gradle.kts
settings.gradle.kts
app/src/main/AndroidManifest.xml
```

## Building (requires Android Studio / SDK 34+, a device with a working NPU)

```bash
cd lens-android
./gradlew :app:assembleDebug   # gradle wrapper not committed; generate with `gradle wrapper`
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Model: export the Vision track's detector to ONNX (see
`../docs/MODEL-GOVERNANCE.md` for the artifact policy) and push it:

```bash
adb push models/vombit_yolov10n_int8.onnx /data/local/tmp/periscope_lens.model
```

## Running (transport: adb forward)

```bash
adb forward tcp:8742 tcp:8742          # phone listens; PC connects
# on the PC, the L6 receiver connects to 127.0.0.1:8742
```

The app prefers `adb forward` (no extra permissions beyond USB debugging),
falls back to USB-tethering RNDIS, and never uses wireless for the assist
path (100–200 ms — see the latency budget in `docs/LENS-TOPOLOGY.md`).

## ROI discipline (what makes 60 Hz feasible)

- Full-frame inference only for *acquisition*; a crosshair-centered ROI
  (from the PC's `State` message or the last-known target) for tracking
  frames.
- Frame-diff gating skips inference when the ROI is unchanged beyond a
  threshold (static HUD, pause menus).
- The capture timestamp published in each `Targets` frame is the Camera2
  `sensorTimestamp` — L7 compensation depends on it, never on send time.

## Benchmark table (fill before any performance claim)

| device | SoC/NPU | model | input | full-frame fps | ROI fps | p50/p95 infer ms | notes |
|---|---|---|---|---|---|---|---|
| *(fill at L5 acceptance)* | | | | | | | |

## Known gaps (honest list)

- **The Camera2 capture loop and the ORT session are seams, not
  implementations.** The activity probes capabilities and runs the
  transport listener; no frames are produced yet. The surrounding modules
  (TargetsEncoder, TransportHub, HidGadgetHelper) fix the interfaces the
  loop must feed.
- The Gradle wrapper is not committed (build requires a local Gradle 8+).
- The ncnn/QNN NPU path is not wired; the shipped code uses ONNX Runtime
  Mobile with NNAPI execution provider as the first NPU route.
- No instrumented tests yet; `TargetsEncoder` is deliberately pure so a JVM
  unit test can mirror the Python decoder's fixtures when CI lands.
- HID gadget support is feature-detected at runtime and degrades to
  targets-only with a user-visible message (L8 rule: never silent).
