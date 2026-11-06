# 157_auditor_introspection_spoof - Auditor introspection interplay

Family: Evasion. Tiers: T1-T2. Area: t2. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Lie to the in-game auditor's introspection - rewrite the CRC its module scan reports, null the diagnostic dumps that overlap the cheat image, and feed original prologue bytes back through the string-copy helper

Blue: Stop trusting single introspection reads - verify the CRC report against itself (CRC-of-CRC), read prologues through a second path, and cross-check dumps

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::auditor_introspection_spoof::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Require the auditor posture (`diagnostic_system_active` + `diagnostic_state.is_active`, seeded in `pair.cpp` like sibling pairs).
2. Map the cheat image inside the game: `auditor_spoof.dat` module + `manual_mapped_region`.
3. Rewrite the module-scan CRC report: push a `DiagnosticModuleInfo` whose `crc32` is the pristine expected value; set `auditor_crc_report_spoofed`, `auditor_crc_rewrites = 2` (initial scan + re-scan).
4. Null the diagnostic dumps overlapping the image: `diagnostic_dump_nulled_overlap`, `diagnostic_dump_nulls = 1`; the dump still runs (`diag_memory_dump_performed`) - it was sanitized, not blocked.
5. Feed the saved original prologue back through the string-copy helper: `prologue_string_copy_feedback = true`.
6. Verify: the module still reports `text_hash == "clean"` while the manual-mapped region and all three spoof scars persist.

Real-world mechanism mapped to lab scars:

- VAC-style auditors hash module ranges and compare against expected CRCs;
  in-game introspection is read from inside the trusted process, so red
  subverts the read path itself. The lab models the three classic subversions
  as `auditor_crc_report_spoofed` (report rewrite), `diagnostic_dump_nulled_overlap`
  (dump sanitization), and `prologue_string_copy_feedback` (RtlCopyString-style
  helpers returning cached original bytes).

Achieved when: all three spoof scars persist while the auditor sees a clean report.

## BLUE

Entry: `examples::auditor_introspection_spoof::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (CRC-of-CRC): re-verify the CRC report itself - rewritten reports fail self-verification (`auditor_crc_of_crc_checked`).
2. Reason 2 (second-path prologue read): re-read the prologue via a path red did not subvert (`auditor_second_path_prologue_read`).
3. Reason 3 (diagnostic cross-check): dumps overlapping a live mapped region must not be uniformly nulled.
4. Reason 4: execution-artifact corroboration (manual-mapped region under a clean report).
5. Mitigation: persist both verification paths and accumulate `evidence_count`; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && crc_of_crc && second_path && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

An auditor that introspects from inside the process it audits can be fed lies
through the very read paths it uses. The fix is redundancy: hash the report
(CRC-of-CRC), read the same bytes twice through independent paths, and
cross-check dumps against the module map - a single spoofed read cannot
satisfy all three.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 157_auditor_introspection_spoof
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `auditor_introspection_spoof/red_example.cpp` - full red multi-step
- `auditor_introspection_spoof/blue_example.cpp` - full blue multi-reason
- `auditor_introspection_spoof/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
- `cs2/diagnostics.hpp` (DiagnosticModuleInfo / DiagnosticTelemetryState)
