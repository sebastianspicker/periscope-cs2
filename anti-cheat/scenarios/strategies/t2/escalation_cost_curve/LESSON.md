# 164_escalation_cost_curve - Escalation-cost curve

Family: Detection. Tiers: T1-T2. Area: t2. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Climb the access rungs - RPM, then direct syscalls, then a kernel driver - each rung buys better access but leaves distinct persistent system mutations

Blue: Observe the mutation footprints (testsigning flag, HVCI/blocklist registry toggles, pending reboot, driver, handle scars) and price the escalation into a ranked deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::escalation_cost_curve::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process.
2. Rung 1 (RPM): `escalation-rpm.exe` opens a plain VM_READ handle and reads the entity count - no posture mutation, but a handle scar.
3. Rung 2 (syscall): `escalation-syscall.exe` opens via the syscall path (`via_syscall_path`) and reads - a different acquisition-model scar.
4. Rung 3 (kernel driver): load `escalation-memrw.sys` + a mem-rw device, and pay the entry fee:
   `trust.test_signing = true`, `trust.hvci_enabled = false`,
   `blocklist_registry_toggled`, `registry_persisted_config` (CI\Config key),
   `pending_reboot_required`.
5. Read through the device IOCTL.
6. Price it: `escalation_footprints` counts the persistent mutations (6 here).

Real-world mechanism mapped to lab scars:

- Every escalation rung leaves boot/OS-level residue: bcdedit
  testsigning, HVCI/Smart App Control toggles, the Microsoft vulnerable
  driver blocklist registry keys, and a pending reboot - all observable long
  after the cheat unloads. The lab maps them onto `HostTrust.test_signing`,
  `HostTrust.hvci_enabled`, `blocklist_registry_toggled` /
  `registry_persisted_config`, and `pending_reboot_required`.

Achieved when: all three rungs read entity data and >= 4 persistent footprints exist.

## BLUE

Entry: `examples::escalation_cost_curve::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Footprint 1 (strategy scar): a non-AC memrw driver is loaded (kernel rung reached).
2. Footprint 2: testsigning enabled. 3: HVCI disabled. 4: blocklist/CI registry toggles persisted. 5: pending reboot. 6: lower-rung handle scars (winapi + syscall edges).
3. Pricing: the escalation-cost curve - by the kernel rung the footprints cost more than the access is worth; at `footprints >= 4` and high risk blue denies ranked play outright.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && footprints >= 4 && risk >= 0.70` (ranked deny)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Escalation is not free: the deeper the access rung, the more permanent the
system mutations it needs. A blue that inventories posture mutations
(testsigning, HVCI, blocklist keys, pending reboots) prices any escalation
after the fact - the driver can unload, but the posture it required stays
behind as evidence.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 164_escalation_cost_curve
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `escalation_cost_curve/red_example.cpp` - full red multi-step
- `escalation_cost_curve/blue_example.cpp` - full blue multi-reason
- `escalation_cost_curve/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
- sibling lessons: `t1/dse_testsign`, `t3/hvci_race` (posture scars in isolation)
