# 152_uefi_txt_measured_boot — UEFI TXT measured boot evasion

Family: Evasion. Tiers: crosscutting. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Modify the TPM event log post-measured-launch to hide an early component; spoof PCRs to match

Blue: Verify TPM event log integrity — recalculate PCRs from log, compare against signed quotes, detect entry removal

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::uefi_txt_measured_boot::Red::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Measured-boot evasion — remove early-load measurement entries and spoof PCR 17-18 values so the machine still claims to pass attestation.
2. Direct World trust/note mutations (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Leave Trust.attestation_valid = true (still claims to pass attestation).
- Set Trust.attestation_pcr_ok = false (PCR reconstruction from the log would fail).
- Set Trust.secure_launch = false (measured launch no longer active).

World scars and lab surfaces (from shipped red code):
- Trust.attestation_pcr_ok — flipped to false.
- Trust.secure_launch — cleared.
- Trust.attestation_valid — kept true, creating a claim-vs-evidence contradiction.
- w.note() — TPM event-log read / entry-removal residuals.

Achieved when: `!w.trust.attestation_pcr_ok || !w.trust.secure_launch`

## BLUE

Entry: `examples::uefi_txt_measured_boot::Blue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: PCR value mismatch — recalculated measurements differ from the quote.
2. Narrator counter: Secure launch / measured boot not active.
3. Narrator counter: Attestation claims valid despite PCR mismatch — the contradiction is itself a signal.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `pcr_mismatch`
- result field `event_log_tampered`
- result field `detection_count`
- checks Trust.attestation_pcr_ok / secure_launch / attestation_valid

Win conditions for this pair:
- detected := `blue.detection_count > 0`
- mitigated := `mitigation.detection_count > 0` (Blue::mitigate fails remote attestation and flags the component)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Measured boot is a chain-of-trust mechanism: the event log must reconstruct the PCRs, and the PCRs must match a signed quote. Red cannot keep the log, the PCRs, and the attestation claim consistent at once, which is exactly the contradiction blue is designed to catch — and why failing attestation is the appropriate response.

## Run

```bash
./build/strategy_lab run 152_uefi_txt_measured_boot
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `uefi_txt_measured_boot/red_example.cpp` — full red multi-step
- `uefi_txt_measured_boot/blue_example.cpp` — full blue multi-reason
- `uefi_txt_measured_boot/pair.cpp` — StrategyEntry wiring + narrator
