# 08_manual_map_hide — Manual map / PEB unlink

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Hide module from PEB lists

Blue: Scan private executable regions + threads, not only PEB

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::manual_map_hide::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Manual map + unlink — Map PE without LoadLibrary; erase headers; no PEB link.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- inject_module() — Module inject (manual_map clears PEB link when true).
- spawn() — Spawn actor process on World process list.
- text_hash="foreign" — Module.text_hash set to foreign (integrity scar)
- linked_in_peb — linked_in_peb=true scar

Achieved when: `present && !peb_linked && region_flag && foreign_thread`

## BLUE

Entry: `examples::manual_map_hide::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Weak PEB walk — Misses unlinked modules.
2. Narrator counter: Memory region + thread heuristics — Private RX, erased headers, foreign threads.
3. Sensor A: weak PEB / module-list walk (only linked_in_peb entries).
4. Sensor B: memory region + thread origin heuristics.
5. Weak PEB sensor alone would miss; deep sensors constrain the technique.
6. local `peb_sees_hidden` init=false
7. local `unlinked` init=0
8. local `erased_headers` init=0
9. local `region_hit` init=g->manual_mapped_region
10. local `thread_hit` init=g->has_foreign_thread
11. local `deep_hit` init=region_hit || thread_hit || unlinked > 0 ||
                        erased_headers > 0
12. local `peb_blind` init=!peb_sees_hidden && (unlinked > 0 || region_hit)

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `peb_sees_hidden` init=false
- local `unlinked` init=0
- local `erased_headers` init=0
- local `region_hit` init=g->manual_mapped_region
- local `thread_hit` init=g->has_foreign_thread
- local `deep_hit` init=region_hit || thread_hit || unlinked > 0 ||
                        erased_headers > 0
- local `peb_blind` init=!peb_sees_hidden && (unlinked > 0 || region_hit)
- checks Process inject/hijack/hollow scars
- checks PEB link / erased headers

Win conditions for this pair:
- detected := `deep_hit`
- mitigated := `peb_blind && deep_hit`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (inject_module() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 08_manual_map_hide
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `manual_map_hide/red_example.cpp` — full red multi-step
- `manual_map_hide/blue_example.cpp` — full blue multi-reason
- `manual_map_hide/pair.cpp` — StrategyEntry wiring + narrator
