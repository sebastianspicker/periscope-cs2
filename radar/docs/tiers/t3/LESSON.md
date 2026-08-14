# T3 Cat-and-Mouse Lesson

Battlefield: Personal HV / trust / attestation (sim HostTrust). Sim only.

## RED goal

Use the shipped team library `t3_red::HvRadar` (and strategy-specific red_example paths) to plant multi-step World scars and achieve an information or control advantage.

## BLUE goal

Use `t3_blue::PlatformAc` and pair blue_example sensors for multi-reason detect and/or mitigate. Pass is blue_detected || blue_mitigated || !red_achieved.

## Strategy pairs in this tier

- `37_attestation` — strategies/attestation/LESSON.md
- `27_boot_trust` — strategies/boot_trust/LESSON.md
- `82_ci_options` — strategies/ci_options/LESSON.md
- `59_cr3_stealth_target` — strategies/cr3_stealth_target/LESSON.md
- `49_efi_boot_entry` — strategies/efi_boot_entry/LESSON.md
- `58_elam_bypass` — strategies/elam_bypass/LESSON.md
- `48_ept_hide_ac` — strategies/ept_hide_ac/LESSON.md
- `83_feature_control_msr` — strategies/feature_control_msr/LESSON.md
- `71_hvci_race` — strategies/hvci_race/LESSON.md
- `05_hypervisor` — strategies/hypervisor/LESSON.md
- `94_infinity_hook` — strategies/infinity_hook/LESSON.md
- `36_nested_hv` — strategies/nested_hv/LESSON.md
- `70_secure_kernel_view` — strategies/secure_kernel_view/LESSON.md
- `38_timing_spoof` — strategies/timing_spoof/LESSON.md
- `95_vtl1_enclave_miss` — strategies/vtl1_enclave_miss/LESSON.md

## How to read a pair

1. LESSON.md (this folder's strategies/<name>/) — multi-step red scars and blue sensors.
2. red_example.cpp — exact World mutations and team calls.
3. blue_example.cpp — multi-reason detect/mitigate.
4. pair.cpp — StrategyEntry id, family, narrator.

## Run

```bash
./build/strategy_lab run --tier T3
./build/duel_t3   # if built
```

## Takeaway

Personal HV attacks HostTrust. Blue answers with attestation, dual-view, timing multi-leaf, and session deny — not usermode RPM alone.
