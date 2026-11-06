# NOTICE

Periscope is a monorepo umbrella. Subtrees keep **separate** licenses. There is no single SPDX identifier for the whole repository.

## Dual licensing

| Path | License | SPDX | Canonical text |
|------|---------|------|----------------|
| `radar/` | MIT | MIT | [`radar/LICENSE`](radar/LICENSE) |
| `vision/` | GNU Affero General Public License v3 **only** | AGPL-3.0-only | [`vision/LICENSE`](vision/LICENSE) |

When you copy, modify, or redistribute code, follow the license of the subtree that owns that code. Do not assume MIT terms apply to `vision/`, or AGPL terms to `radar/`.

Root-level documentation and policy files (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, this file, root `.gitignore`) describe the umbrella. They do not relicense either project.

## Third-party notes (vision)

Vision pulls optional training/inference backends and research datasets. Obligations stay separate from the package license. Summary:

- **Ultralytics** — optional train/infer path; Ultralytics software and default weights are AGPL-3.0 with separate commercial licensing available.
- **CS2-10k** and similar third-party datasets — own terms (for example CC BY-NC 4.0); do not assume commercial redistribution of derived weights without a rights review.
- Game content, base weights, and other frameworks remain their authors’ property.

Full project notice: [`vision/NOTICE.md`](vision/NOTICE.md). Dataset and model license callouts also appear in [`vision/README.md`](vision/README.md) and [`vision/docs/DATASET.md`](vision/docs/DATASET.md).

## Third-party notes (radar)

Radar is MIT for the lab sources under `radar/`. External tools, SDKs, and any game binaries you attach to in real mode are outside this tree and keep their own terms. See [`radar/LICENSE`](radar/LICENSE) and [`radar/SECURITY.md`](radar/SECURITY.md).

Adapted upstream designs and code (2026-11-05 research pass, see
[`docs/ADAPTATION_LEDGER.md`](docs/ADAPTATION_LEDGER.md) for provenance):

- **tiansongyu/cs2_cheat** — MIT. The player-sample hygiene policies
  (`radar/src/domain/radar/sampling.hpp`, ledger R2) are a port of
  `radar_player_sample.hpp`; the snapshot/protocol and fixed-map radar design
  (R1, R4, R5, R6, R7) follow its documented architecture.
- **cs2-dma/CS2-DMA** — Apache-2.0. The snapshot-ring/scene-watermark
  concurrency design (R3), the web-radar service shape and runtime stats
  (R5), and the offset-sync workflow hardening (R8) are adapted from its
  sources. Apache-2.0 NOTICE obligations are honored here.
- **staromodny/miata** — license verified per-tree before code reuse. The
  view-relative mini-radar math (R12) is a port of `src/main.cpp`.
- **avitran0/deadlocked**, **dkrutsko/oasis**, **chao-shushu/CS2-DMA**,
  **Read1dno/vesta** — designs referenced; verify each LICENSE before any
  code (not just design) is copied (ledger X1).
- **IMXNOOBX/cs2-external-esp** — CC BY-NC 4.0: reference-only, no code
  reuse (ledger X3).

Vision/lens third-party references and verdicts (2026-11-05/06 research
passes; ledger X1/X5 — each entry records whether code, design, or nothing
was taken):

- **scrcpy** — Apache-2.0. *Invoked, not linked*: the L2 phone-camera
  transport shells out to the scrcpy binary; no source is vendored.
- **ncnn** (nihui/ncnn-android-yolov8 template) — BSD-3-clause. *Design
  reference* for the Android companion (L5); no code copied. If the Android
  app later links ncnn, its notice obligations land here.
- **ONNX Runtime Mobile** — MIT. Declared dependency of the Android
  companion (`lens-android/app/build.gradle.kts`); resolved by Gradle, not
  vendored.
- **Arian04/android-hid-client** — GPL (verify the exact variant before any
  reuse). *Mechanism reference only*: the configfs USB-gadget approach is
  standard Android and was reimplemented from public documentation.
- **arXiv:2004.08030** (smartphone camera pointer) and **arXiv:2602.01559**
  (MIRAGE flicker/moiré) — cited and reimplemented; no code or dataset
  shipped.
- **Aimmy (Babyhamsta), ReaSnow S1, NobleAIM** — product/design references
  only.
- **DroidCam / Iriun / Camo** — proprietary clients; supported as
  *user-installed* options, never bundled or redistributed.
