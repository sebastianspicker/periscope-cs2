# 150_uefi_secureboot_bypass — UEFI Secure Boot bypass

Family: Evasion. Tiers: T2. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Disable Secure Boot or install a custom db entry to load unsigned drivers

Blue: Monitor UEFI variables, verify driver signatures, re-enable Secure Boot via firmware callback

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::uefi_secureboot_bypass::Red::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Secure Boot bypass via SetVariable — disable Secure Boot and drop the DSE policy so unsigned drivers can load.
2. Direct World trust/note mutations (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Set Trust.secure_boot = false when it was previously enabled (SecureBoot var modified).
- Set Trust.dse_enforced = false and mark a custom `db` entry installed.

World scars and lab surfaces (from shipped red code):
- Trust.secure_boot — flipped to false (SecureBoot var).
- Trust.dse_enforced — cleared, allowing unsigned drivers.
- w.note() — "secure_boot_variable_modified=true" / "uefi_variable_name=db" residuals.

Achieved when: `red.secure_boot_disabled || red.unsigned_driver_allowed`

## BLUE

Entry: `examples::uefi_secureboot_bypass::Blue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Secure Boot state change is the primary scar.
2. Narrator counter: UEFI variable watchlist — known variables are SecureBoot, SetupMode, db, dbx, KEK, PK.
3. Narrator counter: Driver signature verification — an unsigned driver under DSE is a hard signal.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `secure_boot_status_changed`
- result field `uefi_variable_modified`
- result field `unsigned_driver_detected`
- result field `detection_count`
- inspects Trust.secure_boot
- inspects World.drivers
- checks World.module_is_signed

Win conditions for this pair:
- detected := `blue.detection_count > 0`
- mitigated := `mitigation.detection_count > 0` (Blue::mitigate re-enables secure_boot and dse_enforced via firmware callback)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

UEFI variable state and Secure Boot posture are boot-level invariants that an OS anti-cheat cannot hide from — Secure Boot bypasses are a firmware-class attack with a firmware-class fix. The pair models the correct response: re-enable the policy through a firmware callback and invalidate attestation, not a usermode patch.

## Run

```bash
./build/strategy_lab run 150_uefi_secureboot_bypass
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `uefi_secureboot_bypass/red_example.cpp` — full red multi-step
- `uefi_secureboot_bypass/blue_example.cpp` — full blue multi-reason
- `uefi_secureboot_bypass/pair.cpp` — StrategyEntry wiring + narrator
