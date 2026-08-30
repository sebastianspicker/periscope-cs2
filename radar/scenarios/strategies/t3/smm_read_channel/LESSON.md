# 62_smm_read_channel — SMM read channel

Family: Delivery. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Plant SMM handler for ring -2 memory read, bypassing HV

Blue: Detect via SMM residual + HV anomaly; mitigation requires firmware support

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::smm_read_channel::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: SMM read channel — clear VBS/HVCI trust, start a coordinator HV, load an SMM bridge driver, and read via a ring -2 residual; no game handle.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Clear Trust.vbs / hvci / hvci_enabled, then try_start_personal_hv("LabSmmCoordinator").
- spawn() an `smm-bridge-ui.exe` actor process.
- load_driver() an `smmbridge.sys` image scar; create_device() a `\Device\SmmBridge` node.
- Set World.smm_read_channel_planted, World.smm_residual, Trust.unexpected_efi_entry and Trust.efi_entry_name.
- hv_read() a 4-byte entity sample through the bridge; increment World.smm_read_ops.
- Verify World.handles_to(game) stays empty.

World scars and lab surfaces (from shipped red code):
- load_driver() — Load Driver into World.drivers (kernel image scar).
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- smm_read_channel_planted — World.smm_read_channel_planted = true.
- smm_residual — World.smm_residual = true.
- unexpected_efi_entry — Trust.unexpected_efi_entry = true.
- spawn() — Spawn actor process on World process list.

Achieved when: `smm_read_channel_planted && entities_read && smm_read_ops > 0 && no_game_handle`

## BLUE

Entry: `examples::smm_read_channel::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: No OS handle to catch — the read happens below the kernel in SMM.
2. Narrator counter: Firmware-class residual + trust posture — the only signals are SMM/EFI residuals and disabled VBS/HVCI.

Team / depth sensors:
- Direct World reads + local risk scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- checks World.smm_read_channel_planted / smm_residual
- checks Trust.personal_hv_active / vbs / hvci / hvci_enabled
- checks Trust.unexpected_efi_entry

Win conditions for this pair:
- detected := `smm_read_channel_planted || (smm_residual && personal_hv)`
- mitigated := `risk >= 0.8` (blue flips World.ranked_access_denied; no firmware monitor exists in the lab)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

SMM reads sit below both the OS and the hypervisor, so blue cannot rely on handle telemetry at all. In practice the defense is platform firmware support — SMM monitoring and boot-level attestation — and this pair explicitly models the lab's lack of a firmware monitor by making high-confidence policy denial the only post-compromise response.

## Run

```bash
./build/strategy_lab run 62_smm_read_channel
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `smm_read_channel/red_example.cpp` — full red multi-step
- `smm_read_channel/blue_example.cpp` — full blue multi-reason
- `smm_read_channel/pair.cpp` — StrategyEntry wiring + narrator
