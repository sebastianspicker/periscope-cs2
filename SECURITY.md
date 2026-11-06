# Security

Periscope is an umbrella for CS2-related research projects. Security expectations differ by subtree.

## Scope

| Subtree | What it is | Notes |
|---------|------------|-------|
| [`anti-cheat/`](anti-cheat/) | Simulation-only tiered red/blue lab: evasion and detection lessons over a simulated world | Real drivers, firmware and live backends live in `radar/`; not a production anti-cheat |
| [`radar/`](radar/) | Game-facing radar product: real adapters, live CS2 reading, web radar and relay (optional Windows real backends) | Live game usage is in scope ([decision 0002](docs/decisions/0002-live-game-use-permitted.md)); high-impact backends default off; not a production anti-cheat |
| [`vision/`](vision/) | Pre-alpha pixel-only outlining (screen / capture device / local video) | Current input is external pixels; memory access and input automation are permitted directions ([decision 0002](docs/decisions/0002-live-game-use-permitted.md)) that this tree does not implement yet |
| [`lens/`](lens/) | Pre-alpha phone-camera track: a smartphone watches the monitor; portable calibration/transport/policy core + Android source drop | Not an Android release; output paths compute deltas and are opt-in; camera-only capture |

Per-project detail: [`anti-cheat/SECURITY.md`](anti-cheat/SECURITY.md), [`radar/SECURITY.md`](radar/SECURITY.md), [`vision/SECURITY.md`](vision/SECURITY.md), [`lens/SECURITY.md`](lens/SECURITY.md).

### Lens track posture (added 2026-11-06, ledger X6)

The Lens subtree (`lens/`) is camera-first: the phone watches the same
light a human watches; today the game PC runs nothing. Its
output/actuation paths (HID gadget, PC agent - `lens/src/periscope_lens/output.py`,
`lens/src/periscope_lens/policy.py`) compute motion deltas and are opt-in
like radar's real backends; nothing is default-on, and hardware
injectors (kmbox-class) are documented as compatible hardware and are not
yet implemented here. See [`lens/docs/TOPOLOGY.md`](lens/docs/TOPOLOGY.md).

## Reporting

- Prefer a private report to the repository owner when a contact channel exists.
- Do not attach private gameplay frames, account data, API tokens, private model weights, or credentials.

If the only channel is a public issue tracker, describe the repository-side bug (build script surprise, accidental secrets, docs/code mismatch about read-only behavior) without a full exploit write-up.

## Radar: high-risk paths

These exercise privileged OS and hardware paths. Run them where an incident
(test-signing, driver load, DMA configuration) is acceptable - typically a
dedicated machine.

| Path | Risk |
|------|------|
| `radar/examples/drivers/example_vulnerable/` | Vulnerable driver pattern; test-signing |
| `radar/examples/firmware/example_pcie_dma/` | DMA / FPGA host examples |
| `radar/adapters/real/` | Process open, memory read, IOCTL, VMX, DMA helpers when enabled |

High-impact CMake flags (`LR_ENABLE_REAL_*`, kernel/VMX/DMA) default off.

## Vision: capture surfaces

- Capture surfaces: screen (`mss`), OpenCV capture devices, local video files.
- Game-memory access and input automation are permitted directions
  ([decision 0002](docs/decisions/0002-live-game-use-permitted.md)); this tree
  does not implement them yet.
- Prefer ONNX loads with checksum manifests; treat hash mismatches as untrusted.
- Third-party weights and datasets keep their own licenses (see [`NOTICE.md`](NOTICE.md)).

## Operators

You are responsible for platform terms, anti-cheat policy, and local law. Lab code is not a permission slip.
