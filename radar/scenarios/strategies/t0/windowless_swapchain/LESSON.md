# 57_windowless_swapchain — Windowless swapchain hijack

Family: Delivery. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Hook game Present with no visible overlay window

Blue: Detect swapchain hijack via module Present hook + windowless overlay signature

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::windowless_swapchain::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Windowless swapchain hijack — hook the game Present without a topmost/transparent overlay window.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `present-patcher.exe` actor process on the World process list.
- open_process() from the patcher to the game with VmRead|VmWrite (verified in the handle graph).
- read_mem() a 4-byte entity sample through that patch handle.
- Locate the `client.dll`/`game.exe` module and set Module.present_hooked = true.
- Set World.windowless_swapchain_hijack and World.swapchain_hijacked_no_window.
- add_overlay() a swapchain marker that hijacks the swapchain but is neither topmost nor transparent.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- Module.present_hooked — game module Present hook flag.
- windowless_swapchain_hijack — World.windowless_swapchain_hijack = true.
- swapchain_hijacked_no_window — World.swapchain_hijacked_no_window = true.
- add_overlay() — Overlay with hijacks_swapchain, topmost=false, transparent=false.

Achieved when: `present_hooked && windowless_swapchain_hijack && swapchain_hijacked_no_window && windowless_marker && !visible_overlay`

## BLUE

Entry: `examples::windowless_swapchain::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Swapchain hook without an owned visible window is the scar.
2. Direct World reads + local risk scoring in blue_example.cpp.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- inspects Module.present_hooked on game modules
- inspects Overlay hijacks_swapchain / topmost / transparent traits

Win conditions for this pair:
- detected := `(present_hooked || windowless_swapchain_hijack) && signals >= 2`
- mitigated := `risk >= 0.65` (blue flips World.ranked_access_denied)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hiding the overlay window defeats topmost-window heuristics but leaves the Present hook, the swapchain flag, and a foreign VM_READ/VM_WRITE handle behind. Real anti-cheat engines treat module-integrity and windowless rendering scars as independent sensors, and windowless overlays are a known approach for avoiding desktop-window telemetry in legitimate overlays gone wrong.

## Run

```bash
./build/strategy_lab run 57_windowless_swapchain
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `windowless_swapchain/red_example.cpp` — full red multi-step
- `windowless_swapchain/blue_example.cpp` — full blue multi-reason
- `windowless_swapchain/pair.cpp` — StrategyEntry wiring + narrator
