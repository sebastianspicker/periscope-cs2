# Security

## What this project is

Educational red/blue lab (radar track under the Periscope monorepo) for studying external radar techniques and detection. Simulation mode never touches a game. Real mode can attach to processes and exercise low-level Windows / hardware paths when you opt in at configure time.

## What it is not

- Not a supported cheat pack.
- Not a production anti-cheat product.
- Not a toolkit for bypassing HVCI, VBS, Secure Boot, or commercial AC in the wild.

## Lab-only components

| Path | Risk |
|------|------|
| `code/drivers/example_vulnerable/` | Vulnerable driver pattern. Requires test-signing. Lab machines only. |
| `code/firmware/example_pcie_dma/` | DMA host / FPGA examples. Needs special hardware. |
| `code/lib/real/` | Process open, memory read, IOCTL, VMX, DMA helpers when enabled. |

Do not load the sample driver or run elevated real backends on systems you care about.

## Reporting issues

Report vulnerabilities in this track via the Periscope repository security channel (private advisory if the host supports it). Examples in scope: a build script that does something surprising, accidental credential material, or a demo that writes game memory when docs say it only reads. If private reporting is unavailable, open a normal issue without exploit detail.

Please do **not** open issues that are “how do I evade X on live VAC/FACEIT/…”. Those are out of scope.

## Safe defaults

- Default CMake config favors simulation + tests.
- High-impact real backends (`LR_ENABLE_REAL_KERNEL`, `VMX`, `DMA`, …) default **off**.
- Prefer `LR_MODE=sim` when exploring strategies.
