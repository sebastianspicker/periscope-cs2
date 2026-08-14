# 64_iat_eat_hook — IAT / EAT hook scar

Family: Evasion. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Set client.dll iat_hooked and/or eat_hooked on the game process

Blue: Any is_game module with iat_hooked || eat_hooked

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::iat_eat_hook::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: IAT / EAT hook — Rewrite client.dll import/export tables so calls land in cheat code.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- client.dll.iat_hooked — client.dll Module.iat_hooked = true
- client.dll.eat_hooked — client.dll Module.eat_hooked = true

Achieved when: `true after planting: hit=true, Module.iat_hooked=true, Module.eat_hooked=true, client.dll.iat_hooked=true — iat_eat_hook: client.dll iat_hooked=true eat_hooked=true`

## BLUE

Entry: `examples::iat_eat_hook::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: IAT/EAT integrity — Any is_game module with iat_hooked || eat_hooked.
2. local `hits` init=0
3. inspects Process.modules (IAT/EAT/text_hash/present_hooked)
4. checks Module.iat_hooked / eat_hooked

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `hits` init=0
- inspects Process.modules (IAT/EAT/text_hash/present_hooked)
- checks Module.iat_hooked / eat_hooked

Win conditions for this pair:
- detected := `hits > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (client.dll.iat_hooked and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 64_iat_eat_hook
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `iat_eat_hook/red_example.cpp` — full red multi-step
- `iat_eat_hook/blue_example.cpp` — full blue multi-reason
- `iat_eat_hook/pair.cpp` — StrategyEntry wiring + narrator
