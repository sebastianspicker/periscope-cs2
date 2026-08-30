# 65_multi_process_ipc_split — Multi-process IPC split

Family: Evasion. Tiers: T0. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Split the cheat into 3 IPC processes — holder, reader, UI

Blue: Detect via section IPC + handle-process mismatch

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::multi_process_ipc_split::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Multi-process IPC split — one process holds the game handle, another reads, a third renders; no single process looks like the cheat.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() holder `audiodg.exe`, reader `radar-engine.exe`, UI `radar-ui.exe`.
- open_process() from the holder to the game with VmRead; read entity bytes through the holder.
- add_section() holder→reader and reader→ui shared sections carrying entity bytes.
- add_overlay() the radar UI on top.
- Set World.multi_process_split_active / split_holder_pid / split_reader_pid / split_ui_pid.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- add_section() — Shared section edges (creator→consumer, carries entity bytes).
- add_overlay() — Topmost overlay window.
- multi_process_split_active — World.multi_process_split_active = true.
- spawn() — Spawn actor processes on World process list.

Achieved when: `multi_process_split_active && holder_has_handle && !reader_has_handle && !ui_has_handle && reader_to_ui_section`

## BLUE

Entry: `examples::multi_process_ipc_split::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Shared sections carrying entity bytes — the IPC surface.
2. Narrator counter: Handle-process mismatch — the holder differs from the UI, and a system-named process holds the game handle.
3. Narrator counter: audiodg.exe with a VM_READ handle is a classic masquerade tell.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- inspects World.sections (carries_entity_bytes)
- inspects Process.name (audiodg.exe) / split holder / split UI pids

Win conditions for this pair:
- detected := `multi_process_split_active || (section_with_entities && foreign_vm_read_handle)`
- mitigated := `risk >= 0.60` (blue flips World.ranked_access_denied)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Splitting a cheat across processes hides the all-in-one process but creates a richer surface: entity-carrying shared sections, a handle held by a process that does not render, and system-named process masquerade. Real detection correlates handle ownership with IPC graphs, which is exactly the holder/reader/UI mismatch this pair models.

## Run

```bash
./build/strategy_lab run 65_multi_process_ipc_split
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `multi_process_ipc_split/red_example.cpp` — full red multi-step
- `multi_process_ipc_split/blue_example.cpp` — full blue multi-reason
- `multi_process_ipc_split/pair.cpp` — StrategyEntry wiring + narrator
