# Notice - Periscope Lens

Periscope Lens is licensed under the GNU Affero General Public License,
version 3 only (SPDX-License-Identifier: AGPL-3.0-only). The canonical
license text is included in `LICENSE`.

Third-party references (2026-11-05/06 research passes; verdicts carried
from the monorepo root NOTICE when this track split out of `vision/`):

- scrcpy - Apache-2.0. *Invoked, not linked*: the phone-capture
  transport shells out to the scrcpy binary; no source is vendored.
- ncnn (nihui/ncnn-android-yolov8 template) - BSD-3-clause. *Design
  reference* for the Android companion; no code copied.
- ONNX Runtime Mobile - MIT. Declared Android dependency
  (`android/app/build.gradle.kts`), resolved by Gradle, not vendored.
- OpenCV (`opencv-python`) - Apache-2.0 (OpenCV) with an MIT-licensed
  Python wrapper. Optional `capture` extra for the real UVC/DroidCam
  bindings; resolved by uv, not vendored.
- Arian04/android-hid-client - GPL (verify the exact variant before any
  reuse). *Mechanism reference only*: the configfs USB-gadget approach is
  standard Android and was reimplemented from public documentation.
- arXiv:2004.08030 (smartphone camera pointer) and arXiv:2602.01559
  (MIRAGE flicker/moiré) - cited and reimplemented; no code or dataset
  shipped.
- Aimmy (Babyhamsta), ReaSnow S1, NobleAIM - product/design references
  only.
- DroidCam / Iriun / Camo - proprietary clients; supported as
  *user-installed* options, never bundled.

## Derived in-repo material

- `typings/cv2.pyi` is a 5-line boundary stub copied from
  `vision/typings/cv2.pyi` (same AGPL-3.0-only license, same monorepo).
- The wire-format mirror in `android/.../TargetsEncoder.kt` mirrors the
  Python encoder in this package; it is first-party code, duplicated by
  design (two runtimes, one contract).
