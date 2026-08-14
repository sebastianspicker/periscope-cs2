# 100_skin_changer — Skin changer

Family: Feature. Tiers: all. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Plant inventory/paint-kit mismatch scars on diagnostic convar + PE stamp fields

Blue: Multi-reason detection on attribute violations (paint kit, client stamp)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::skin_changer::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Skin changer — modify diagnostic convar and PE-timestamp fields so inventory paint-kit attributes no longer match the shipped client.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Set DiagnosticState.convar_crc_mode1 = 0xDEADBEEF (modifies the convar CRC).
- XOR the expected client DLL PE timestamp into a mismatched value.

World scars and lab surfaces (from shipped red code):
- DiagnosticState.convar_crc_mode1 — modified from expected_convar_crc_mode1.
- DiagnosticState.pe_timestamp_client_dll — modified from expected_pe_timestamp_client_dll.
- w.note() — "skin_changer: paint kit / inventory attrs modified" residual.

Achieved when: `w.diagnostic_state` attributes are modified (pair marks red_achieved = true after apply).

## BLUE

Entry: `examples::skin_changer::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Convar CRC mismatch — the paint-kit attribute no longer matches the expected diagnostic state.
2. Narrator counter: PE timestamp mismatch — the client stamp attribute is wrong.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `violations`
- result field `attrs` (paint_kit / client_stamp)
- result field `detected`
- compares DiagnosticState.convar_crc_mode1 vs expected_convar_crc_mode1
- compares DiagnosticState.pe_timestamp_client_dll vs expected_pe_timestamp_client_dll

Win conditions for this pair:
- detected := `r.violations > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Client-side cosmetic changes leave integrity fingerprints in diagnostic state: the convar CRC and the client DLL timestamp must stay consistent with the shipped build. Real anti-cheat integrity checks compare runtime diagnostic fields against expected values, which is exactly the mismatch pair this strategy simulates.

## Run

```bash
./build/strategy_lab run 100_skin_changer
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `skin_changer/red_example.cpp` — full red multi-step
- `skin_changer/blue_example.cpp` — full blue multi-reason
- `skin_changer/pair.cpp` — StrategyEntry wiring + narrator
