# 156_eprocess_derivation - Runtime structural offset derivation vs layout churn

Family: Evasion. Tiers: T0-T1. Area: t1. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Derive structure offsets at runtime from self-validating invariants (list-cycle walk, DTB self-probe, PEB Ldr cycle) - nothing offset-shaped ships in the binary, and a layout churn is survived by re-deriving

Blue: Detect the derivation probe's read-scar burst and the version-churn pressure runtime derivation creates, then re-baseline on every churn

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::eprocess_derivation::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its planted entity table.
2. Spawn an `eprocess-derive-actor.exe` lab actor.
3. Invariant pass 1 - three self-validating probes, each costing read scars:
   `derivation_list_cycle_ok` (ActiveProcessLinks-style cycle closes),
   `derivation_dtb_probe_ok` (DTB self-probe validates the directory base),
   `derivation_peb_ldr_cycle_ok` (PEB Ldr InLoadOrderModuleList cycle closes).
4. Activate `structural_derivation_active` - offsets are derived, none shipped.
5. Prove the derived offset: read the entity count at the derived table and decode it.
6. Version churn: `mutate_lab_pattern_layout` moves the marker/table and bumps
   `lab_pattern_generation`; red re-derives (`derivation_churn_rescans++`) and
   re-reads to prove drift resilience.

Real-world mechanism mapped to lab scars:

- Real derivations validate candidate EPROCESS offsets by walking a list and
  checking it cycles back to the head, probing the DirectoryTableBase, and
  walking the PEB loader lists. The lab models the probe cost as
  `derivation_probe_reads` (3 per pass) and the churn response as
  `derivation_churn_rescans` - the same way a live tool re-derives after a
  game update instead of shipping a new offset dump.

Achieved when: all three invariants validated and the post-churn re-derivation read entity data.

## BLUE

Entry: `examples::eprocess_derivation::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): `structural_derivation_active` is set.
2. Reason 2: the probe pattern - `derivation_probe_reads >= 6` (two passes) is a burst no normal reader produces.
3. Reason 3: version-churn pressure - `derivation_churn_rescans > 0` proves offsets are derived at runtime, so blue's own offset baselines must be re-validated every build.
4. Reason 4: an untrusted mem-rw driver that could host the probes (inventory corroboration).
5. Mitigation: continuous re-validation - blue bumps `pattern_rescan_count`, re-baselining its layout knowledge on churn, and ranked-denies at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && risk >= 0.65`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Runtime derivation removes the "shipped offsets" artifact scanners hunt for,
but the derivation itself is loud: invariant probing is a repeating read
burst, and every layout churn forces another burst. Blue trades one-time
signature scans for continuous churn-correlated probe detection.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 156_eprocess_derivation
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `eprocess_derivation/red_example.cpp` - full red multi-step
- `eprocess_derivation/blue_example.cpp` - full blue multi-reason
- `eprocess_derivation/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
