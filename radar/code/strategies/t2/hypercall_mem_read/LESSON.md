# 69_hypercall_mem_read — Hypercall memory read

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read physical memory through platform hypervisor hypercalls, no driver

Blue: Detect via HV-level monitoring + hypercall interface analysis

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::hypercall_mem_read::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Hypercall memory read — an exposed hypercall page returns synthetic entity data; no driver, no game handle.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `hypercall-reader.exe` actor process.
- Require Trust.platform_hv_active and a platform_hv_vendor; bail if VBS is enabled.
- Set World.hypercall_read_active and World.hypercall_vendor.
- Copy the synthetic entity count out of the game memory target page.
- Verify the handle graph stays empty of game VM_READ handles.

World scars and lab surfaces (from shipped red code):
- hypercall_read_active — World.hypercall_read_active = true.
- hypercall_vendor — World.hypercall_vendor = platform vendor.
- spawn() — Spawn actor process on World process list.

Achieved when: `hypercall_read_active && entity_count > 0 && no_game_vm_read_handle`

## BLUE

Entry: `examples::hypercall_mem_read::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Handle telemetry is blind — the hypercall path leaves no game VM_READ handle.
2. Narrator counter: HV posture — an active platform hypervisor with VBS disabled makes the interface more accessible.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph (expects empty for a clean hypercall path)
- checks Trust.platform_hv_active / platform_hv_vendor
- checks Trust.vbs

Win conditions for this pair:
- detected := `hypercall_read_active || (platform_hv_active && !vbs && !handle_exists)`
- mitigated := `risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypercall-based reads move the delivery scar from the kernel into the hypervisor interface, so blue must reason at HV level and over trust posture rather than the handle graph. VBS/HVCI is a meaningful mitigation because it restricts access to the platform hypercall surface — the pair explicitly models VBS as a blocker for the red path.

## Run

```bash
./build/strategy_lab run 69_hypercall_mem_read
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `hypercall_mem_read/red_example.cpp` — full red multi-step
- `hypercall_mem_read/blue_example.cpp` — full blue multi-reason
- `hypercall_mem_read/pair.cpp` — StrategyEntry wiring + narrator
