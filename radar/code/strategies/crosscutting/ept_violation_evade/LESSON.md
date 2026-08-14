# 70_ept_violation_evade — EPT violation side-channel evade

Family: Evasion. Tiers: T3. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Use EPT violation timing to detect and evade AC memory scans

Blue: Detect via INVEPT residual + HV probe (near impossible)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::ept_violation_evade::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: EPT timing side channel — clear VBS/HVCI, start a coordinator HV, trap AC accesses with EPT, and time violation windows to dodge scan sweeps.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Clear Trust.vbs / hvci / hvci_enabled, then try_start_personal_hv("LabEptTimingCoordinator").
- spawn() an `ept-timing-bridge-ui.exe` actor process.
- Set World.ept_sidechannel_active, World.invept_tlb_flush_used, World.ept_ac_scan_evasions = 3.
- hv_read() a 4-byte entity sample through the bridge; keep the game handle graph empty.

World scars and lab surfaces (from shipped red code):
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- ept_sidechannel_active — World.ept_sidechannel_active = true.
- invept_tlb_flush_used — World.invept_tlb_flush_used = true.
- ept_ac_scan_evasions — World.ept_ac_scan_evasions = 3.
- spawn() — Spawn actor process on World process list.

Achieved when: `personal_hv_active && ac_scans_evaded == 3 && used_invept && entities_read && no_game_handle`

## BLUE

Entry: `examples::ept_violation_evade::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: EPT side-channel residual is the strongest reason — but it requires HV introspection to see.
2. Narrator counter: INVEPT flushes and evaded scan windows corroborate from the HV side.

Team / depth sensors:
- Direct World reads + local risk scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- checks World.ept_sidechannel_active / invept_tlb_flush_used / ept_ac_scan_evasions
- checks Trust.personal_hv_active / vbs / hvci / hvci_enabled

Win conditions for this pair:
- detected := `ept_sidechannel_active || (personal_hv && invept_used)`
- mitigated := `risk >= 0.8` (blue flips World.ranked_access_denied; EPT timing resembles ordinary virtualization overhead)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

EPT violation timing is a hypervisor-resident evasion: the guest OS sees nothing, and even the HV sees only timing that resembles normal virtualization overhead. Detection is effectively limited to HV introspection and INVEPT accounting, and this pair is explicit that blue's confidence floor is high because the residual is so easy to confuse with benign overhead.

## Run

```bash
./build/strategy_lab run 70_ept_violation_evade
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `ept_violation_evade/red_example.cpp` — full red multi-step
- `ept_violation_evade/blue_example.cpp` — full blue multi-reason
- `ept_violation_evade/pair.cpp` — StrategyEntry wiring + narrator
