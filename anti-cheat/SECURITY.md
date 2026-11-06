# Security

## What this project is

Simulation-only teaching lab (anti-cheat track under the Periscope monorepo): a tiered red/blue lab over a deterministic, CS2-shaped `sim::World`. Nothing in this tree touches a game, a process, a device, or a network - the tier model (T0–T4) is a *simulation* played out on World fields, and the `LAB_ALLOW_*` flags widen simulated surfaces only.

## What it is not

- Not a supported cheat pack.
- Not a production anti-cheat product.
- Not a toolkit for bypassing HVCI, VBS, Secure Boot, or commercial AC in the wild.
- Not a source of real drivers, firmware, or live backends.

## Real-path material lives in radar

This tree contains no sample driver, no firmware, and no real backends. The
real-path, high-impact material is product-side, lives in the radar tree, and
keeps its posture there:

| Path (radar tree) | Risk |
|------|------|
| `../radar/examples/drivers/example_vulnerable/` | Vulnerable driver pattern. Requires test-signing. Lab machines only. |
| `../radar/examples/firmware/example_pcie_dma/` | DMA host / FPGA examples. Needs special hardware. |
| `../radar/adapters/real/` | Process open, memory read, IOCTL, VMX, DMA helpers when enabled. |

Do not load the sample driver or run elevated real backends on systems you care about. See [`../radar/SECURITY.md`](../radar/SECURITY.md).

## Reporting issues

Report vulnerabilities in this track via the Periscope repository security channel (private advisory if the host supports it). Examples in scope: a build script that does something surprising, accidental credential material, or a doc/code mismatch about simulation-only behavior. If private reporting is unavailable, open a normal issue without exploit detail.

Please do **not** open issues that are "how do I evade X on live VAC/FACEIT/…". Those are out of scope.

## Safe defaults

- The only configuration is simulation; the tree configures and builds with no real flags.
- The tier ladder (T0–T4) is a simulated delivery model over `sim::World`, not executable evasion.
- The host test (`tests/stack_posture_test.cpp`) exercises posture policy logic only; the live posture decision it protects is documented in [docs/STEALTH-POSTURE.md](docs/STEALTH-POSTURE.md), and the live implementation lives in the radar tree. The test is skipped when the sibling radar checkout is absent; it is not a runtime dependency.
