CS2 Vision Access is licensed under the GNU Affero General Public License,
version 3 only (SPDX-License-Identifier: AGPL-3.0-only).

The canonical license text is included in `LICENSE`.

This project uses Ultralytics software as an optional training and inference
backend. Ultralytics publishes that software and its trained models under
AGPL-3.0 by default, with separate commercial licensing available. Dataset,
model-weight, game-content, and framework licenses remain separate obligations.

Lens-track third-party verdicts moved with the subtree split: see
[`../lens/NOTICE.md`](../lens/NOTICE.md).

Model-weight licenses shipped through the model registry (see
`docs/MODEL-GOVERNANCE.md` for the governance policy):

- **Ultralytics YOLO11 weights** — AGPL-3.0; derived weights stay AGPL.
- **Vombit CS2 detectors** (`vombit-yolov10*`) — CC-BY-NC-ND-4.0:
  non-commercial, no derivatives. Do not redistribute modified weights or
  use them commercially without a rights review.
- **EdgeSAM encoder/decoder** (chongzhou96/EdgeSAM) — NTU S-Lab License 1.0:
  redistribution permitted for **non-commercial** use with attribution;
  verified against the upstream LICENSE on 2026-11-06 (the registry
  previously recorded "MIT", which was wrong — the V9 hygiene gate now
  exists so this class of error is caught by review of the governance diff).
