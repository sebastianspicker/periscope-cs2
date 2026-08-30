# 60_physmem_direct_read — PhysicalMemory direct read

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Map PhysicalMemory directly, no handle to game

Blue: Detect physmem device open + DSE policy check

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::physmem_direct_read::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: PhysicalMemory direct read — open the physmem device and map the game PFN; no game VM_READ handle.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `mem-mapper.exe` actor process.
- Bail when Trust.dse_enforced blocks PhysicalMemory access.
- Set World.physmem_device_open and World.physmem_direct_mapped.
- Set World.physmem_game_pfn from the game base page frame number.
- Copy the synthetic entity count out of the game memory entity table.
- Verify the handle graph stays empty of game VM_READ handles.

World scars and lab surfaces (from shipped red code):
- physmem_device_open — World.physmem_device_open = true.
- physmem_direct_mapped — World.physmem_direct_mapped = true.
- physmem_game_pfn — World.physmem_game_pfn = game_base >> 12.
- spawn() — Spawn actor process on World process list.

Achieved when: `physmem_direct_mapped && entities_read && no_game_vm_read_handle`

## BLUE

Entry: `examples::physmem_direct_read::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Physmem device open is the scar — direct physical mapping leaves no VM_READ handle.
2. Narrator counter: DSE policy — when DSE is enforced, PhysicalMemory access is blocked and mitigation is policy-driven.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph (expects empty for a direct physical path)
- checks World.physmem_direct_mapped / physmem_device_open
- checks Trust.dse_enforced

Win conditions for this pair:
- detected := `(physmem_direct_mapped || physmem_device_open) && !dse_enforced && signals >= 2`
- mitigated := `(dse_enforced && (physmem_direct_mapped || physmem_device_open)) || risk >= 0.7`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Direct PhysicalMemory mapping is the classic handle-free kernel read, and DSE is the primary defense because it blocks the driver that would open the device. Blue wins here on device-open telemetry plus policy: with DSE enforced, the physical path becomes a mitigation story rather than a detection one.

## Run

```bash
./build/strategy_lab run 60_physmem_direct_read
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `physmem_direct_read/red_example.cpp` — full red multi-step
- `physmem_direct_read/blue_example.cpp` — full blue multi-reason
- `physmem_direct_read/pair.cpp` — StrategyEntry wiring + narrator
