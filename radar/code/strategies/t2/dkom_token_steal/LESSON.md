# 61_dkom_token_steal — DKOM token steal

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Steal a SYSTEM token via DKOM for an unrestricted game handle

Blue: Detect token anomaly + full-access handle monitoring

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dkom_token_steal::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DKOM token steal — load a lab driver, copy the SYSTEM token, then open a full-access handle to the game.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `syshelper.exe` actor process.
- load_driver() a `dkom-token-lab.sys` kernel image scar.
- Set World.dkom_token_stolen, World.dkom_token_source_pid = 4 (SYSTEM), World.token_bypasses_handle_acls.
- open_process() from syshelper to the game with VmRead|VmWrite|VmOperation.
- read_mem() a 4-byte entity sample through the full-access handle.

World scars and lab surfaces (from shipped red code):
- load_driver() — Load Driver into World.drivers (kernel image scar).
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- dkom_token_stolen — World.dkom_token_stolen = true.
- dkom_token_source_pid — World.dkom_token_source_pid = 4.
- token_bypasses_handle_acls — World.token_bypasses_handle_acls = true.
- spawn() — Spawn actor process on World process list.

Achieved when: `dkom_token_stolen && dkom_token_source_pid == 4 && token_bypasses_handle_acls`

## BLUE

Entry: `examples::dkom_token_steal::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Full-access handle is the delivery scar — VmRead+VmWrite+VmOperation on a non-AC process.
2. Narrator counter: Token anomaly — a SYSTEM token assigned to a non-SYSTEM process is itself suspicious.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph
- filters AccessMask::VmRead / VmWrite / VmOperation handles
- inspects Process.name / is_ac / is_game
- checks World.dkom_token_stolen / token_bypasses_handle_acls / dkom_token_source_pid

Win conditions for this pair:
- detected := `dkom_token_stolen || full_access_handle`
- mitigated := `risk >= 0.75`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Token theft converts the OS handle ACL into an asset: a full-access handle no longer looks like a normal RPM reader. Real security software correlates handle access masks with process privilege and token source, and treats a non-SYSTEM process holding a SYSTEM-derived token as a hard integrity violation.

## Run

```bash
./build/strategy_lab run 61_dkom_token_steal
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `dkom_token_steal/red_example.cpp` — full red multi-step
- `dkom_token_steal/blue_example.cpp` — full blue multi-reason
- `dkom_token_steal/pair.cpp` — StrategyEntry wiring + narrator
