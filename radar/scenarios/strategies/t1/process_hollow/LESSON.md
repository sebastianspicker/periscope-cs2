# 55_process_hollow — Process hollowing

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Hollow svchost.exe (looks_reputable), map region, open VM_READ

Blue: Detect hollowed or (reputable + manual_mapped + foreign VM_READ)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::process_hollow::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Process hollow — Hollow svchost.exe façade; map region; OpenProcess(VM_READ).

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- Process.looks_reputable — Process.looks_reputable = true
- Process.hollowed — Process.hollowed = true
- Process.original_image — Process.original_image = "C:\\Windows\\System32\\svchost.exe"
- Process.manual_mapped_region — Process.manual_mapped_region = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0 && p->hollowed && p->looks_reputable && p->manual_mapped_region && vm_read`

## BLUE

Entry: `examples::process_hollow::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Hollow scar — Detect hollowed=true / original_image mismatch.
2. Narrator counter: Reputation + map + handle — looks_reputable + manual_mapped_region + foreign VM_READ.
3. local `hollowed_hits` init=0
4. local `combo_hits` init=0
5. local `hollow_scar` init=p.hollowed
6. local `foreign_vm_read` init=false
7. reads handle graph via handles_to()
8. checks Process inject/hijack/hollow scars
9. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `hollowed_hits` init=0
- local `combo_hits` init=0
- local `hollow_scar` init=p.hollowed
- local `foreign_vm_read` init=false
- reads handle graph via handles_to()
- checks Process inject/hijack/hollow scars
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `hollowed_hits > 0 || combo_hits > 0`
- mitigated := `hollowed_hits > 0 || combo_hits > 0`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (Process.looks_reputable and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 55_process_hollow
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `process_hollow/red_example.cpp` — full red multi-step
- `process_hollow/blue_example.cpp` — full blue multi-reason
- `process_hollow/pair.cpp` — StrategyEntry wiring + narrator
