# 31_parent_lineage — Parent lineage spoof

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Launch under explorer/Discord so process tree looks benign

Blue: Join handle graph with parent lineage; flag shell→cheat edges

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::parent_lineage::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Parent lineage spoof — Spawn radar under explorer.exe/Discord.exe; syscall OpenProcess.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- Process.looks_reputable — Process.looks_reputable = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0 && syscall_handle && parent_ok`

## BLUE

Entry: `examples::parent_lineage::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Parent reputation alone — Trusted shell parent is not a free pass.
2. Narrator counter: Lineage anomaly — Suspicious name or VM_READ under unrelated trusted shell.
3. Path A: foreign VM_READ whose parent is an unrelated trusted shell.
4. Path B: suspicious-named child of a trusted shell (even before handle sample).
5. Mitigated: parent reputation alone is insufficient; lineage+handle joins.

Team / depth sensors:
- `depth::MultiSampleHandleDetector`
  - push multiple sample(w, i) then evaluate: composed_hit, race_detected, continuous_hit.
  - Not a single World bool — composition across samples + lineage + continuous truth.
- call `det.push()`
- call `det.sample()`
- call `det.evaluate()`

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `lineage_hits` init=0
- local `vm_read_under_shell` init=0
- local `suspicious_under_shell` init=0
- local `already` init=false
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `lineage_hits > 0 || (ms.lineage_hit || ms.composed_hit)`
- mitigated := `vm_read_under_shell > 0`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (Process.looks_reputable and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 31_parent_lineage
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `parent_lineage/red_example.cpp` — full red multi-step
- `parent_lineage/blue_example.cpp` — full blue multi-reason
- `parent_lineage/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/handle_multisample.hpp`
