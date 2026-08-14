# 32_inmatch_only — In-match-only reader

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Open game handle only while match_active; stay dark in lobby

Blue: Sample during match: foreign VM_READ or reader_active flag

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::inmatch_only::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: In-match-only attach — Spawn reader in lobby; open VM_READ only when match_active.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- match_active — World.match_active = true
- Process.reader_active — Process.reader_active = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0 && has_handle && active_flag && w.match_active`

## BLUE

Entry: `examples::inmatch_only::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Lobby-only sampling — Missing the match window misses the handle.
2. Narrator counter: In-match sensors — Foreign VM_READ or reader_active during match → detect.
3. Detect when match is live and either foreign VM_READ or reader_active.
4. Mitigated: timing-aware sample during match window still catches attach.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `foreign_vm_read` init=0
- local `reader_active_hits` init=0
- local `in_match` init=w.match_active
- local `handle_hit` init=in_match && foreign_vm_read > 0
- local `active_hit` init=in_match && reader_active_hits > 0
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `handle_hit || active_hit`
- mitigated := `out.detected && in_match`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (match_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 32_inmatch_only
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `inmatch_only/red_example.cpp` — full red multi-step
- `inmatch_only/blue_example.cpp` — full blue multi-reason
- `inmatch_only/pair.cpp` — StrategyEntry wiring + narrator
