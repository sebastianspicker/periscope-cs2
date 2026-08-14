# Gap inventory — sophisticated red/blue strategies

**Date:** 2026-07-21 (wave 6 re-diff)  
**Live catalog:** **87** strategies (`01`–`87`) via `./build/strategy_lab list`.

Sources: `docs/TIERED-COUNTERS.md`, `docs/OVERVIEW.md`, `docs/THREAD-SUMMARY.md`, `docs/T3-HYPERVISOR.md`.

## Live implemented (do not re-do)

| Range | Summary |
|-------|---------|
| 01–28 | Core delivery + classic features/evasion/structural |
| 29–63 | Co-run through VEH (waves 1–3) |
| 64–75 | IAT/EAT, Present, mapper, DSE, DKOM, physmem, SK view, HVCI race, desktop dup, multibox, overwatch, polymorphic |
| 76–87 | GDI BitBlt, WH_MOUSE, ThreadHide, speedhack, pool-tag, WFP/NDIS, CI options, FEATURE_CONTROL, lag switch, display clone, VPN graph, watermark |

## Wave 6 — still missing (in-scope lab)

| New ID | Tier | Red | Blue | Status |
|--------|------|-----|------|--------|
| **88_printwindow** | T0 | PrintWindow capture residual | Capture API scar + fog | **done** |
| **89_block_input** | T0 | BlockInput residual | Input-block policy detect | **done** |
| **90_sedebug_priv** | T1 | SeDebugPrivilege enabled | Privilege inventory | **done** |
| **91_raw_vs_sendinput** | T1 | Mix raw_hid + SendInput desync | Input source consistency | **done** |
| **92_instr_callback** | T2 | Instrumentation callback hook residual | Instr-callback integrity | **done** |
| **93_etw_ti_blind** | T2 | ETW-TI consumer blind (deeper ETW) | ETW-TI pipeline check | **done** |
| **94_infinity_hook** | T3 | InfinityHook-class syscall hook residual | Syscall-hook residual detect | **done** |
| **95_vtl1_enclave_miss** | T3 | Expected VTL1/enclave missing | VTL1/enclave required policy | **done** |
| **96_packet_loss_disambig** | T4 | Fake packet loss vs lag-switch | Disambiguate loss vs switch | **done** |
| **97_clipcursor** | T4 | ClipCursor confinement residual | Cursor confinement detect | **done** |
| **98_overwatch_multisignal** | XC/ops | Multi-weak-signal delayed legit | Multi-signal overwatch score | **done** |
| **99_clipboard_token** | XC/evasion | Clipboard token/leak residual | Clipboard monitor | **done** |

## Per-tier wave-6 notes

| Tier | After 87? | Action |
|------|-----------|--------|
| T0 | PrintWindow + BlockInput | 88, 89 |
| T1 | SeDebug + raw vs SendInput | 90, 91 |
| T2 | Instr callback + ETW-TI | 92, 93 |
| T3 | InfinityHook residual + VTL1 miss | 94, 95 |
| T4 | Packet-loss disambig + ClipCursor | 96, 97 |
| XC | Overwatch multi-signal + clipboard | 98, 99 |

## Out of scope

Real VMX/EPT, real BYOVD, physical DMA/FPGA, SMM/firmware, real InfinityHook payloads.

## Prior waves

Waves 1–5 (`29`–`87`): **done**.

## Depth pedagogy pass (no catalog growth)

Post-wave-6 work deepened multi-step composition without new flag-only IDs:

- `shared/depth/*` — handle multi-sample, leakage scorer, multi-invariant scorer, trust aggregator, seller fusion, residual research (VMX/BYOVD/DMA/SMM)
- `depth_tests` ctest target
- Docs: `docs/RESIDUAL-VMX-BYOVD-DMA-SMM.md`
- Catalog remains 99
