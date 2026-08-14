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
