# 160_zero_run_carving - Zero-run carving vs VAD-vs-PTE consistency audit

Family: Evasion. Tiers: T2. Area: t2. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Find verified zero pages inside the target's existing mappings and place code there - no VirtualAllocEx/NtMapViewOfSection-class API ever runs, so allocation-API telemetry sees nothing

Blue: Audit the exploitable gap: pages that are PTE-valid and now non-zero but absent from the VAD manager's reservations

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::zero_run_carving::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its memory image (arena plants entities; the region beyond the table is a zero run).
2. Spawn a `zero-carve-actor.exe` lab actor.
3. Verify the zero run at offset 0x400 (all-zero page range inside the existing image).
4. Carve: `zero_run_carve_active`, `zero_run_pages_carved = 1`, `zero_run_bytes_written = 0x100`, and - critically - `zero_run_allocation_api_used = false` (no allocation API).
5. Write the payload through the World mutator and verify readback.
6. Leave the gap: `zero_run_vad_reserved = false` - the placement is not reserved in the VAD manager, which is exactly the state blue's audit hunts.

Real-world mechanism mapped to lab scars:

- Carving into a mapped-but-zero region (PE header padding, .bss slack,
  unused sections) leaves PTE-valid pages whose bytes changed with no
  matching allocation event. The VAD manager (the kernel's reservation map)
  never learned about the placement - `zero_run_vad_reserved = false` and
  `zero_run_vad_mismatch_pages` model that gap.

Achieved when: code persists in the zero pages with no allocation API and no VAD reservation.

## BLUE

Entry: `examples::zero_run_carving::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): the carve residual is active.
2. Reason 2 (the gap): VAD-vs-PTE mismatch - `zero_run_vad_mismatch_pages > 0` mapped pages carry no VAD reservation.
3. Reason 3: no allocation API record backs the new bytes.
4. Reason 4: the image-content delta is unexplained by any module/section event.
5. Mitigation: reserve/flag audited pages in the VAD manager (`zero_run_vad_reserved = true`) so the next audit catches any unreserved placement; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && risk >= 0.65`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Skipping the allocation API skips allocation telemetry, but it also skips the
VAD manager - and that omission is a first-class artifact. A walk that
compares PTE-valid, non-zero pages against VAD reservations finds carved code
without ever scanning for a signature.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 160_zero_run_carving
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `zero_run_carving/red_example.cpp` - full red multi-step
- `zero_run_carving/blue_example.cpp` - full blue multi-reason
- `zero_run_carving/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
