# Security

Periscope is an umbrella for two CS2-related research projects. Security expectations differ by subtree.

## Scope

| Subtree | What it is | What it is not |
|---------|------------|----------------|
| [`radar/`](radar/) | Educational red/blue lab for external radar techniques and detection (sim + optional Windows real backends) | Not a supported cheat pack; not a production anti-cheat; not a toolkit for bypassing commercial AC in the wild |
| [`vision/`](vision/) | Pre-alpha pixel-only outlining (screen / capture device / local video) | Not process injection, not game-memory access, not aim or input automation |

Per-project detail: [`radar/SECURITY.md`](radar/SECURITY.md), [`vision/SECURITY.md`](vision/SECURITY.md).

## Reporting

- Prefer a **private** report to the repository owner when a contact channel exists.
- Do **not** open public issues that are “how do I evade X on live VAC/FACEIT/…” or that include exploit recipes for live anti-cheat.
- Do **not** attach private gameplay frames, account data, API tokens, private model weights, or credentials.

If the only channel is a public issue tracker, describe the repository-side bug (build script surprise, accidental secrets, docs/code mismatch about read-only behavior) without a full exploit write-up.

## Radar: lab-only high-risk paths

These exist for teaching. Use them only on disposable lab machines.

| Path | Risk |
|------|------|
| `radar/code/drivers/example_vulnerable/` | Vulnerable driver pattern; test-signing |
| `radar/code/firmware/example_pcie_dma/` | DMA / FPGA host examples |
| `radar/code/lib/real/` | Process open, memory read, IOCTL, VMX, DMA helpers when enabled |

High-impact CMake flags (`LR_ENABLE_REAL_*`, kernel/VMX/DMA) default **off**. Prefer simulation when exploring strategies.

## Vision: external capture only

- Capture surfaces: screen (`mss`), OpenCV capture devices, local video files.
- No CS2 process open, no memory read/write, no input injection in this tree.
- Prefer ONNX loads with checksum manifests; treat hash mismatches as untrusted.
- Third-party weights and datasets keep their own licenses (see [`NOTICE.md`](NOTICE.md)).

## Operators

You are responsible for platform terms, anti-cheat policy, and local law. Lab code is not a permission slip.
