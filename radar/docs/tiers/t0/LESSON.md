# T0 Cat-and-Mouse Lesson

Battlefield: Usermode RPM / inject / overlay scars. Sim only.

## RED goal

Use the shipped team library `t0_red::CheatClient` (and strategy-specific red_example paths) to plant multi-step World scars and achieve an information or control advantage.

## BLUE goal

Use `t0_blue::AcAgent` and pair blue_example sensors for multi-reason detect and/or mitigate. Pass is blue_detected || blue_mitigated || !red_achieved.

## Strategy pairs in this tier

- `89_block_input` — strategies/block_input/LESSON.md
- `65_dxgi_present_hook` — strategies/dxgi_present_hook/LESSON.md
- `01_external_rpm` — strategies/external_rpm/LESSON.md
- `30_fp_allowlist_evasion` — strategies/fp_allowlist_evasion/LESSON.md
- `76_gdi_bitblt` — strategies/gdi_bitblt/LESSON.md
- `42_handle_hide_on_enum` — strategies/handle_hide_on_enum/LESSON.md
- `17_handle_minimize` — strategies/handle_minimize/LESSON.md
- `64_iat_eat_hook` — strategies/iat_eat_hook/LESSON.md
- `07_internal_inject` — strategies/internal_inject/LESSON.md
- `21_module_integrity` — strategies/module_integrity/LESSON.md
- `88_printwindow` — strategies/printwindow/LESSON.md
- `29_process_cooccurrence` — strategies/process_cooccurrence/LESSON.md
- `18_read_throttle` — strategies/read_throttle/LESSON.md
- `43_section_map` — strategies/section_map/LESSON.md
- `53_thread_hijack` — strategies/thread_hijack/LESSON.md
- `77_wh_mouse_hook` — strategies/wh_mouse_hook/LESSON.md

## How to read a pair

1. LESSON.md (this folder's strategies/<name>/) — multi-step red scars and blue sensors.
2. red_example.cpp — exact World mutations and team calls.
3. blue_example.cpp — multi-reason detect/mitigate.
4. pair.cpp — StrategyEntry id, family, narrator.

## Run

```bash
./build/strategy_lab run --tier T0
./build/duel_t0   # if built
```

## Takeaway

T0 is won by blue if the handle graph and inject/module sensors work. Red's answer is not better packing — stop having a handle (T2) or hide under HV (T3).
