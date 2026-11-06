# 158_loader_addr_registry - Loader-resolved encrypted address registry

Family: Delivery. Tiers: T1. Area: t1. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Ship no scanner at all - a loader resolves entity addresses out-of-process into an encrypted registry section; the live process only decodes pointers at read time, and re-resolves when offsets drift

Blue: Detect the decode-time data-section patch, the decode access burst, and the encrypted registry artifact; randomize decode-window re-checks

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::loader_addr_registry::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its planted entity table.
2. Spawn a `registry-loader.exe` loader actor.
3. Arm the encrypted registry: `encrypted_addr_registry_active = true`, `addr_registry_section_encrypted = true` - no scanner code ever ships in the live payload.
4. Open the loader's VM_READ edge (delivery is ordinary RPM; the registry is the point).
5. Decode-time access: `registry_decode_ops = 3`; decoding writes plaintext pointers into the data section (`registry_data_section_patched = true`) for the read window.
6. Read the entity count through the registry-guided address.
7. Sub-scenario - offset drift: `mutate_lab_pattern_layout` churns the layout; the live binary cannot scan, so the loader re-resolves out-of-process (`registry_drift_recoveries++`, `offset_drift_recovered`) and the post-drift read succeeds.

Real-world mechanism mapped to lab scars:

- Real encrypted registries live in a PE section (or appended blob) that is
  decrypted only at decode time - a section artifact blue can enumerate - and
  the decode writes plaintext pointers into `.data`, a short-lived mutation.
  The lab models the registry as `addr_registry_section_encrypted`, the
  decode as `registry_decode_ops` + `registry_data_section_patched`, and the
  drift response as `registry_drift_recoveries`.

Achieved when: registry-guided reads decoded entity data before and after the drift.

## BLUE

Entry: `examples::loader_addr_registry::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): the data section was patched at decode time - plaintext pointers that were never in the on-disk image.
2. Reason 2: decode-time access burst (`registry_decode_ops >= 3`) - pointer-table accesses cluster in a short window.
3. Reason 3: an encrypted non-image registry section is present.
4. Reason 4: drift-recovery telemetry - loader re-resolves after churn, the same shape as schema C2 updates.
5. Reason 5: remote-read telemetry from the registry-guided reads.
6. Mitigation: randomized decode-window re-checks (`pattern_rescan_count++`) so the decoded window cannot stay hidden; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Removing the scanner removes the scan signature but not the registry: an
encrypted section, a decode-time data-section patch, and bursty decode
accesses are all new artifacts. Offset drift is also a blue sensor - the
re-resolve traffic looks exactly like the schema-C2 shape blue already hunts.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 158_loader_addr_registry
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `loader_addr_registry/red_example.cpp` - full red multi-step
- `loader_addr_registry/blue_example.cpp` - full blue multi-reason
- `loader_addr_registry/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
