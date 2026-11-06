# Security

## What this project is

The game-facing radar product tree of the Periscope monorepo: real adapters, live CS2 reading, the web radar and the relay. Its simulation demos run on the anti-cheat lab's simulated world - the lab is simulation-only and has its own posture notes ([../anti-cheat/SECURITY.md](../anti-cheat/SECURITY.md)); the simulation never touches a game. Real mode can attach to processes and exercise low-level Windows / hardware paths when you opt in at configure time. Live game usage is in scope ([../docs/decisions/0002-live-game-use-permitted.md](../docs/decisions/0002-live-game-use-permitted.md)).

## Position

- Research tooling, not a packaged consumer product.
- Not a production anti-cheat product.

## Higher-risk components

| Path | Risk |
|------|------|
| `examples/drivers/example_vulnerable/` | Vulnerable driver pattern. Requires test-signing. Run where test-signing is acceptable. |
| `examples/firmware/example_pcie_dma/` | DMA host / FPGA examples. Needs special hardware. |
| `adapters/real/` | Process open, memory read, IOCTL, VMX, DMA helpers when enabled. |

The sample driver requires test-signing; run elevated real backends on a machine where that is acceptable.

## Reporting issues

Report vulnerabilities in this track via the Periscope repository security channel (private advisory if the host supports it). Examples in scope: a build script that does something surprising, accidental credential material, or a demo that writes game memory when docs say it only reads. If private reporting is unavailable, open a normal issue without exploit detail.

## Safe defaults

- Default CMake config favors simulation.
- High-impact real backends (`LR_ENABLE_REAL_KERNEL`, `VMX`, `DMA`, …) default off.

## Field context (recorded 2026-11-06, ledger X4 - not a policy source)

An upstream maintainer reported on 2026-10-09 that Valve's servers appeared
to be running a VACnet model with high recall and low precision: triggerbot,
aimbot and recoil-control usage triggered cooldowns, and *kill timing* (the
delta between an opponent's first visibility and the kill) looked like a
detection signal even for apparently legitimate play. One author's
unverified observation - recorded here because it is consistent with this
project's standing posture decision: read-only/pixel-only/camera-only
surfaces are the low-risk half of the design space, and everything that
acts (input synthesis, memory writes) is lab-gated and opt-in. Treat it
as context, not authority. (The standing posture decision this paragraph
references was superseded on 2026-11-06 by
[../docs/decisions/0002-live-game-use-permitted.md](../docs/decisions/0002-live-game-use-permitted.md);
the observation is kept as field context.)
