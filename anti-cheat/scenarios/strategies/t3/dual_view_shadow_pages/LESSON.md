# 159_dual_view_shadow_pages - Dual-view shadow pages (execute patched / read pristine)

Family: Evasion. Tiers: T2-T3. Area: t3. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

Extends `48_ept_hide_ac`: that pair hides AC pages from the guest; this pair
keeps the same guest-consistency trick but applies it *within one page* - the
execute view and the read view diverge - and asks what blue can still probe.

## Battlefield

Red: Split the same guest pages into an execute view that runs patched code and a read view that still returns pristine bytes, so self-hashes and self-reads stay clean

Blue: Probe cross-view consistency - timing deltas between fetch and read paths, TLB/ITC flush effects, and forced shootdown re-reads

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dual_view_shadow_pages::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Establish the `48_ept_hide_ac` base posture: `trust.personal_hv_active`,
   `trust.ept_hide_ac_pages` (personal HV remapping).
2. Split the views: `dual_view_shadow_active`, `dual_view_exec_patched = true`
   (fetches see the patch), `dual_view_read_pristine = true` (data reads see
   the original bytes).
3. Guest self-check: `dual_view_cross_reads++`; the read view returns pristine
   bytes and `client.dll` still hashes `text_hash == "clean"`.
4. Verify the split is real: the patch executes while every data read -
   including integrity self-reads - reports the pristine image.

Real-world mechanism mapped to lab scars:

- EPT/NPT dual-view tricks (or SMM-era shadowing) keep two backing pages per
  guest page: instruction fetches fault in the shadow, data reads see the
  original. The lab models this as the `dual_view_*` field family; the
  `ept_hide_ac` fields remain the base posture this lesson builds on.

Achieved when: patched code executes while the guest's own self-read and self-hash stay pristine.

## BLUE

Entry: `examples::dual_view_shadow_pages::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): a dual-view shadow is active with a patched execute view.
2. Reason 2 (self-read probe): exec/read disagreement - pristine reads under a patched execute view.
3. Reason 3 (TLB/ITC probe): forced INVEPT/ITC-style flush leaves observable effects (`dual_view_tlb_effects`).
4. Reason 4 (timing probe): cross-view fetch-vs-read latency delta (`dual_view_probe_delta_ns` vs `baseline_latency_ns`).
5. Reason 5: the EPT/personal-HV base posture corroborates a hypervisor class.
6. Mitigation: persistent shootdown re-reads - repeated flush + dual re-read; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && tlb_effects observed && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Dual-view shadowing breaks the assumption that "what you read is what
executes". Timing, TLB/ITC effects, and shootdown re-reads measure the *cost*
of maintaining two views - red can keep the views consistent for any single
probe, but not for all of them at once, forever.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 159_dual_view_shadow_pages
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `dual_view_shadow_pages/red_example.cpp` - full red multi-step
- `dual_view_shadow_pages/blue_example.cpp` - full blue multi-reason
- `dual_view_shadow_pages/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
- sibling lesson: `t3/ept_hide_ac/LESSON.md` (the extended base pair)
