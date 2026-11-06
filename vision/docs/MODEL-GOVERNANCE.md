# Model governance (ledger V9)

Every model artifact this project loads carries provenance: **source URL,
pinned SHA-256, license, and task**. The machine-readable half is
[`application/configuration/data/model_registry.py`](../src/cs2_vision_access/application/configuration/data/model_registry.py);
its hygiene is gated by [`tests/test_model_governance.py`](../tests/test_model_governance.py)
in CI. This document is the human-readable half and the policy record.

## Trust rules

1. **Default path = verified path.** Models loaded through the registry are
   verified against their pinned SHA-256 before first use (see
   `application/model_assets/manifest.py`). A swapped or corrupted artifact
   fails loudly.
2. **Explicit overrides are deliberate customs.** An explicit
   `sam_*_path` / `CS2_SAM_*` override is a custom export chosen by the
   operator: it must exist and is logged as unpinned, not silently treated
   as verified (A6 judgement call, THINGS_TO_DO_LEDGER.md).
3. **Registry entries cannot skip fields.** A new entry without a well-formed
   SHA-256, license, task, and HTTPS URL fails the hygiene test — the A6
   incident class (a model running "verified" without verification) cannot
   regrow through the registry.
4. **License notes move with the license tag.** Changing a `license` value in
   the registry requires updating this document and `vision/NOTICE.md` in the
   same commit (enforced by `test_registry_license_notes_match_notice`).

## Current artifact families

| Family | License | Redistribution note |
|---|---|---|
| Ultralytics YOLO11 (e.g. `yolo11n-seg`) | AGPL-3.0 | Same license family as this package (`vision/` is AGPL-3.0-only); derived weights stay AGPL. Record any future commercial Ultralytics licensing separately. |
| Vombit CS2 detectors | CC-BY-NC-ND-4.0 | **Non-commercial, no derivatives**: fine for this research use; do not redistribute modified weights or use them in a commercial pipeline without a rights review. |
| EdgeSAM encoder/decoder (chongzhou96/EdgeSAM) | NTU S-Lab License 1.0 | Redistribution permitted for **non-commercial** use with attribution (verified against the upstream LICENSE, 2026-11-06; the registry previously said "MIT" — wrong). Re-verify when the SHA changes. |

## Adding a model

1. Pin the exact file (HTTPS URL) and its SHA-256 — never a "latest" redirect
   that can change under you.
2. Record `task`, `classes` (contiguous ids), and `license`.
3. Add search `tags`; never remove an existing entry without a major version
   bump (the registry is append-only by policy).
4. Run the hygiene test; update this table and `vision/NOTICE.md` if the
   license family is new.

## Provenance for training artifacts

Trained/fine-tuned weights produced by this repository's own training
workflows must ship with a generated manifest (`create_manifest`) at
export time; a weights file without a manifest is not a release artifact.
This is the same guarantee the A6 fix added for loading, applied one step
earlier, at production.
