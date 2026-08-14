# 48_ept_hide_ac — EPT-hide AC pages

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Personal HV; EPT remaps AC so guest self-hash stays clean

Blue: Detect ept_hide_ac_pages / personal_hv+hide; ranked deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::ept_hide_ac::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: EPT-hide AC pages under personal HV — Own VMX root; present clean AC pages to guest self-hash.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- trust.ept_hide_ac_pages — HostTrust.ept_hide_ac_pages = true
- Module.text_hash — game module field text_hash="clean"
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- text_hash="clean" — Module.text_hash set to clean (integrity scar)

Achieved when: `red.hv_started && red.ept_hide`

## BLUE

Entry: `examples::ept_hide_ac::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: EPT-hide / higher-trust AC integrity — Guest self-hash can lie under EPT; rank on hide flag + personal HV.
2. local `any_ac` init=false
3. local `any_dirty` init=false
4. inspects Process.modules (IAT/EAT/text_hash/present_hooked)
5. checks Module.text_hash integrity

Team / depth sensors:
- `depth::TrustAggregator`

Multi-reason / result fields and sensors:
- result field `guest_hash_clean`
- result field `ept_hide_hit`
- result field `personal_hv_hide`
- result field `policy_deny`
- local `any_ac` init=false
- local `any_dirty` init=false
- inspects Process.modules (IAT/EAT/text_hash/present_hooked)
- checks Module.text_hash integrity

Win conditions for this pair:
- detected := `ept_hide_hit || personal_hv_hide`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 48_ept_hide_ac
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `ept_hide_ac/red_example.cpp` — full red multi-step
- `ept_hide_ac/blue_example.cpp` — full blue multi-reason
- `ept_hide_ac/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/trust_aggregator.hpp`
