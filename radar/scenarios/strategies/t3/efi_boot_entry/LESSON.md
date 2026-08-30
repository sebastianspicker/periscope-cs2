# 49_efi_boot_entry — Unexpected EFI boot entry

Family: Structural. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Plant cheat-loader.efi; often Secure Boot off

Blue: Detect unexpected_efi_entry; ranked policy deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::efi_boot_entry::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Unexpected EFI boot entry + personal HV — Persist early loader via EFI; SB off; then own VMX root after boot.
2. Step 1: boot scar — non-allowlisted EFI application in the boot order.
3. Step 2: Secure Boot off so unsigned loader can run.
4. Step 3: free VMX after early loader path (personal HV post-EFI).
5. Step 4: thin bridge scar.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.unexpected_efi_entry — HostTrust.unexpected_efi_entry = true
- trust.efi_entry_name — HostTrust.efi_entry_name = r.efi_entry_name
- trust.secure_boot — HostTrust.secure_boot = false
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.unexpected_entry`

## BLUE

Entry: `examples::efi_boot_entry::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: EFI boot-entry + HV multi-sensor — Unexpected EFI is boot persistence; correlate with personal HV / bridge.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `unexpected_hit`
- result field `entry_name`
- result field `personal_hv`
- result field `bridge_hit`
- result field `policy_deny`
- inspects drivers/devices

Win conditions for this pair:
- detected := `unexpected_hit &&
           (personal_hv || bridge_hit || policy_deny || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 49_efi_boot_entry
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `efi_boot_entry/red_example.cpp` — full red multi-step
- `efi_boot_entry/blue_example.cpp` — full blue multi-reason
- `efi_boot_entry/pair.cpp` — StrategyEntry wiring + narrator
