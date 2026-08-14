# 99_clipboard_token — Clipboard token leak

Family: Evasion. Tiers: all. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: leak + loader + CDN/auth net; optional handle/mapper

Blue: Multi-reason leak && (net||loader||handle) — not bool-echo

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::clipboard_token::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Clipboard token leak — Leak token; spawn loader; plant CDN/auth net; optional handle.
2. Step 1: session / launch token left on the clipboard (loader scar).
3. Step 2: spawn loader/helper process that left the token scar.
4. Step 3: C2 / CDN related to loader (auth or payload fetch).
5. Step 4 (optional): open game handle OR mapper flag for delivery path.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- clipboard_token_leak — World.clipboard_token_leak = true
- mapper_process_present — World.mapper_process_present = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.leak && (r.net_planted || r.loader_spawned || r.handle_open)`

## BLUE

Entry: `examples::clipboard_token::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Clipboard + loader correlation — Flag leak only with loader, net, or handle residual.
2. Narrator counter: Multi-reason ops signal — Never bool-echo clipboard_token_leak alone.
3. Multi-reason: leak alone is bool-echo; require supporting residual.
4. local `support` init=r.net_hit || r.loader_process_hit || r.handle_hit
5. reads handle graph via handles_to()
6. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `leak`
- result field `loader_process_hit`
- result field `net_hit`
- result field `handle_hit`
- result field `multi_reason`
- result field `detected`
- local `support` init=r.net_hit || r.loader_process_hit || r.handle_hit
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (clipboard_token_leak and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 99_clipboard_token
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `clipboard_token/red_example.cpp` — full red multi-step
- `clipboard_token/blue_example.cpp` — full blue multi-reason
- `clipboard_token/pair.cpp` — StrategyEntry wiring + narrator
