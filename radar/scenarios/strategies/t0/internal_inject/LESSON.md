# 07_internal_inject — Internal inject

Family: Delivery. Tiers: T0-T1. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: DLL into game process

Blue: Module list / image load notify

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::internal_inject::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Internal DLL inject — Load cheat module into game; hooks / direct structure access.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- inject_module() — Module inject (manual_map clears PEB link when true).
- spawn() — Spawn actor process on World process list.
- text_hash="foreign" — Module.text_hash set to foreign (integrity scar)
- linked_in_peb — linked_in_peb=true scar

Achieved when: `present`

## BLUE

Entry: `examples::internal_inject::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Module list + image notify — Foreign module in PEB; image load callbacks.
2. local `foreign` init=0
3. local `name_hit` init=m.name.find("cheat") != std::string::npos ||
        m.name.find("hack") != std::string::npos
4. local `hash_hit` init=m.text_hash == "foreign" || m.text_hash == "patched"
5. local `unexpected` init=m.linked_in_peb && (name_hit || hash_hit) &&
        m.name != "game.exe" && m.name != "client.dll"
6. local `notify_path` init=w.ac_callback_present && foreign > 0
7. checks Module.text_hash integrity
8. checks PEB link / erased headers

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `foreign` init=0
- local `name_hit` init=m.name.find("cheat") != std::string::npos ||
        m.name.find("hack") != std::string::npos
- local `hash_hit` init=m.text_hash == "foreign" || m.text_hash == "patched"
- local `unexpected` init=m.linked_in_peb && (name_hit || hash_hit) &&
        m.name != "game.exe" && m.name != "client.dll"
- local `notify_path` init=w.ac_callback_present && foreign > 0
- checks Module.text_hash integrity
- checks PEB link / erased headers

Win conditions for this pair:
- detected := `foreign > 0`
- mitigated := `notify_path`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (inject_module() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 07_internal_inject
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T1
```

Open the pair sources beside this lesson:

- `internal_inject/red_example.cpp` — full red multi-step
- `internal_inject/blue_example.cpp` — full blue multi-reason
- `internal_inject/pair.cpp` — StrategyEntry wiring + narrator
