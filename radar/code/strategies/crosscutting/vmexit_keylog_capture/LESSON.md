# 71_vmexit_keylog_capture — VM-exit keylog capture

Family: Feature. Tiers: T3. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Capture keystrokes via VM-exit interception, invisible to guest OS

Blue: Detect via HV probe + bridge driver correlation (extremely hard)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::vmexit_keylog_capture::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: VM-exit keylog — intercept keyboard IRQs at VM-exit (external interrupts) and exfiltrate captured keys to a bridge UI process; the guest OS sees nothing.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Clear Trust.vbs / hvci / hvci_enabled, then try_start_personal_hv("LabVmExitInterruptCoordinator").
- spawn() a `vmexit-keylog-bridge-ui.exe` actor process.
- load_driver() a `vmexitbridge.sys` image scar; create_device() a `\Device\VmExitBridge` node.
- Set World.vmexit_keylog_active and World.vmexit_keys_captured = 15.

World scars and lab surfaces (from shipped red code):
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- load_driver() — Load Driver into World.drivers (kernel image scar).
- create_device() — Create Device node linked to driver.
- vmexit_keylog_active — World.vmexit_keylog_active = true.
- vmexit_keys_captured — World.vmexit_keys_captured = 15.
- spawn() — Spawn actor process on World process list.

Achieved when: `personal_hv_active && vmexit_keylog_active && keys_captured > 0 && bridge_ready && no_game_handle`

## BLUE

Entry: `examples::vmexit_keylog_capture::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: VM-exit keylog residual is the strongest reason — but guest visibility is limited.
2. Narrator counter: The bridge driver/device is the exfiltration path, and VBS/HVCI-disabled posture corroborates.

Team / depth sensors:
- Direct World reads + local risk scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- checks World.vmexit_keylog_active / vmexit_keys_captured
- checks Trust.personal_hv_active / vbs / hvci / hvci_enabled
- inspects World.drivers / devices for the vmexitbridge pair
- reads handle graph (expects no game-process handle)

Win conditions for this pair:
- detected := `vmexit_keylog_active || (personal_hv && bridge && !vbs)`
- mitigated := `risk >= 0.75` (blue flips World.ranked_access_denied; no guest hooks or callbacks exist to observe)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

VM-exit interception moves the keylog below the guest entirely: no usermode hooks, no kernel callbacks, no game driver. Detection requires HV introspection plus correlation of the bridge driver/device, and even then the residual is subtle — which is why the pair models a high-confidence threshold and why VBS/HVCI remains the practical first line of defense.

## Run

```bash
./build/strategy_lab run 71_vmexit_keylog_capture
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `vmexit_keylog_capture/red_example.cpp` — full red multi-step
- `vmexit_keylog_capture/blue_example.cpp` — full blue multi-reason
- `vmexit_keylog_capture/pair.cpp` — StrategyEntry wiring + narrator
