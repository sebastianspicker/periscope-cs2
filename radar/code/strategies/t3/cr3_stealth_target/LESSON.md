# 59_cr3_stealth_target — CR3 stealth target

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Personal HV; target by CR3; hv_read without name-based open

Blue: Detect personal_hv+no VM_READ / bridge+cr3; policy fail

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::cr3_stealth_target::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: CR3 stealth target under personal HV — Disable VBS; target game by CR3 (no name open); bridge + hv_read.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.hv_started && red.cr3_noted && red.no_name_open && red.hv_read_ok`

## BLUE

Entry: `examples::cr3_stealth_target::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: CR3 stealth / personal-HV read without handle — personal_hv + no foreign VM_READ, or bridge+CR3 path → policy fail.
2. local `foreign_vm_read` init=false
3. inspects drivers/devices
4. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `personal_hv`
- result field `no_foreign_vm_read`
- result field `stealth_read_path`
- result field `bridge_hit`
- result field `cr3_targeting`
- result field `policy_fail`
- local `foreign_vm_read` init=false
- inspects drivers/devices
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `stealth_read_path || bridge_hit || cr3_targeting`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 59_cr3_stealth_target
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `cr3_stealth_target/red_example.cpp` — full red multi-step
- `cr3_stealth_target/blue_example.cpp` — full blue multi-reason
- `cr3_stealth_target/pair.cpp` — StrategyEntry wiring + narrator
