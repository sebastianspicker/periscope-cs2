# NOTICE

Periscope is a monorepo umbrella. Subtrees keep separate licenses. There is no single SPDX identifier for the whole repository.

## Four-tree licensing (per-subtree)

| Path | License | SPDX | Canonical text |
|------|---------|------|----------------|
| `anti-cheat/` | MIT | MIT | [`anti-cheat/LICENSE`](anti-cheat/LICENSE) |
| `radar/` | MIT | MIT | [`radar/LICENSE`](radar/LICENSE) |
| `vision/` | GNU Affero General Public License v3 only | AGPL-3.0-only | [`vision/LICENSE`](vision/LICENSE) |
| `lens/` | GNU Affero General Public License v3 only | AGPL-3.0-only | [`lens/LICENSE`](lens/LICENSE) |

When you copy, modify, or redistribute code, follow the license of the subtree that owns that code. Do not assume MIT terms apply to `vision/`/`lens/`, or AGPL terms to `radar/`/`anti-cheat/`.

Root-level documentation and policy files (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, this file, root `.gitignore`) describe the umbrella. They do not relicense any subtree.

## Third-party notes (vision)

Vision pulls optional training/inference backends and research datasets. Obligations stay separate from the package license. Summary:

- Ultralytics - optional train/infer path; Ultralytics software and default weights are AGPL-3.0 with separate commercial licensing available.
- CS2-10k and similar third-party datasets - own terms (for example CC BY-NC 4.0); do not assume commercial redistribution of derived weights without a rights review.
- Game content, base weights, and other frameworks remain their authors’ property.

Full project notice: [`vision/NOTICE.md`](vision/NOTICE.md). Dataset and model license callouts also appear in [`vision/README.md`](vision/README.md) and [`vision/docs/DATASET.md`](vision/docs/DATASET.md).

## Third-party notes (radar)

Radar is MIT for the sources under `radar/`. External tools, SDKs, and any game binaries you attach to in real mode are outside this tree and keep their own terms. See [`radar/LICENSE`](radar/LICENSE) and [`radar/SECURITY.md`](radar/SECURITY.md).

Radar's adapted-upstream attributions (tiansongyu, cs2-dma, miata and others, from the 2026-11-05 research pass)
moved with the subtree split: see [`radar/NOTICE.md`](radar/NOTICE.md).

## Third-party notes (anti-cheat)

The anti-cheat lab is original code with no adapted upstreams; see
[`anti-cheat/NOTICE.md`](anti-cheat/NOTICE.md).

Lens third-party references and verdicts moved with the subtree split:
see [`lens/NOTICE.md`](lens/NOTICE.md). The Vision-only verdicts stay
in [`vision/NOTICE.md`](vision/NOTICE.md).
