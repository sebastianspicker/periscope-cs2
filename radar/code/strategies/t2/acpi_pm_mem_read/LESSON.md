# 68_acpi_pm_mem_read — ACPI PM memory read

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read game entities via ACPI PM-triggered SMI, no driver or handle to game

Blue: Detect SMM residual + firmware monitoring (extremely hard)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::acpi_pm_mem_read::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ACPI PM memory read — writes to PM1_CNT cause a pre-existing SMM handler to copy the entity table into an ACPI scratch buffer; no driver, no game handle.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() an `acpi-reader.exe` actor process.
- Set World.acpi_pm_read_active and World.acpi_smi_trigger_count = 5 to model PM1_CNT SMI triggers.
- Set World.smm_residual to record the firmware-side handler.
- Copy the synthetic entity count out of the game memory scratch region.

World scars and lab surfaces (from shipped red code):
- acpi_pm_read_active — World.acpi_pm_read_active = true.
- acpi_smi_trigger_count — World.acpi_smi_trigger_count = 5.
- smm_residual — World.smm_residual = true.
- spawn() — Spawn actor process on World process list.

Achieved when: `acpi_pm_read_active && smi_count == 5 && entities_obtained && no_game_vm_read_handle`

## BLUE

Entry: `examples::acpi_pm_mem_read::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Handle telemetry is blind — no VM_READ handle exists on the SMM path.
2. Narrator counter: Firmware residual + trust policy — the only remaining signals live in SMM and DSE state.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph (expects empty for a clean SMM path)
- checks Trust.dse_enforced
- checks World.smm_residual

Win conditions for this pair:
- detected := `acpi_pm_read_active || (smm_residual && !handle_exists && !dse_enforced)`
- mitigated := `risk >= 0.75`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

ACPI PM-triggered SMM reads are one of the few paths that remove the OS handle graph entirely, so blue must lean on firmware residual, platform-monitoring, and DSE policy. In practice this is why platform-level attestation and firmware monitors matter: usermode and kernel telemetry alone cannot see the SMI handler.

## Run

```bash
./build/strategy_lab run 68_acpi_pm_mem_read
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `acpi_pm_mem_read/red_example.cpp` — full red multi-step
- `acpi_pm_mem_read/blue_example.cpp` — full blue multi-reason
- `acpi_pm_mem_read/pair.cpp` — StrategyEntry wiring + narrator
