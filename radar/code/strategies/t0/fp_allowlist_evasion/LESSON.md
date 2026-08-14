# 30_fp_allowlist_evasion — FP allowlist evasion

Family: Evasion. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Masquerade as nvidia-overlay + looks_reputable, still OpenProcess(VM_READ)

Blue: Name allowlist alone misses; still detect foreign VM_READ (reputation != safe)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::fp_allowlist_evasion::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: FP allowlist evasion — Rename to nvidia-overlay, claim looks_reputable, still VM_READ.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- Process.looks_reputable — Process.looks_reputable = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0`

## BLUE

Entry: `examples::fp_allowlist_evasion::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Beyond name allowlist — Name allowlist alone misses; foreign VM_READ when reputation != safe.
2. Multi-sample composition: reputation claim + continuous handle truth.
3. local `naive_suppressed` init=0
4. local `foreign_alerts` init=0
5. local `allowlist_vmread_flags` init=0
6. local `name_ok` init=name_on_fp_allowlist(p->name)
7. reads handle graph via handles_to()
8. filters AccessMask::VmRead handles

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
- local `naive_suppressed` init=0
- local `foreign_alerts` init=0
- local `allowlist_vmread_flags` init=0
- local `name_ok` init=name_on_fp_allowlist(p->name)
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `foreign_alerts > 0 || allowlist_vmread_flags > 0 || ms.reputation_evasion || ms.composed_hit`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (Process.looks_reputable and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 30_fp_allowlist_evasion
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `fp_allowlist_evasion/red_example.cpp` — full red multi-step
- `fp_allowlist_evasion/blue_example.cpp` — full blue multi-reason
- `fp_allowlist_evasion/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/handle_multisample.hpp`
