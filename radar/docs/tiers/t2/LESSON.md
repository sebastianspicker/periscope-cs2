# T2 Cat-and-Mouse Lesson

Battlefield: BYOVD / IOCTL / callback strip (sim drivers). Sim only.

## RED goal

Use the shipped team library `t2_red::KernelRadar` (and strategy-specific red_example paths) to plant multi-step World scars and achieve an information or control advantage.

## BLUE goal

Use `t2_blue::KernelAc` and pair blue_example sensors for multi-reason detect and/or mitigate. Pass is blue_detected || blue_mitigated || !red_achieved.

## Strategy pairs in this tier

- `04_byovd` — scenarios/strategies/byovd/LESSON.md
- `35_callback_shadow` — scenarios/strategies/callback_shadow/LESSON.md
- `16_callback_strip` — scenarios/strategies/callback_strip/LESSON.md
- `68_dkom_hide` — scenarios/strategies/dkom_hide/LESSON.md
- `33_driver_allowlist` — scenarios/strategies/driver_allowlist/LESSON.md
- `46_early_load_race` — scenarios/strategies/early_load_race/LESSON.md
- `93_etw_ti_blind` — scenarios/strategies/etw_ti_blind/LESSON.md
- `92_instr_callback` — scenarios/strategies/instr_callback/LESSON.md
- `03_kernel_ioctl` — scenarios/strategies/kernel_ioctl/LESSON.md
- `56_minifilter_strip` — scenarios/strategies/minifilter_strip/LESSON.md
- `47_object_callback_strip` — scenarios/strategies/object_callback_strip/LESSON.md
- `69_physmem_map` — scenarios/strategies/physmem_map/LESSON.md
- `80_pool_tag_hide` — scenarios/strategies/pool_tag_hide/LESSON.md
- `57_registry_notify_strip` — scenarios/strategies/registry_notify_strip/LESSON.md
- `34_scm_service` — scenarios/strategies/scm_service/LESSON.md
- `81_wfp_ndis_filter` — scenarios/strategies/wfp_ndis_filter/LESSON.md

## How to read a pair

1. LESSON.md (this folder's scenarios/strategies/<name>/) — multi-step red scars and blue sensors.
2. red_example.cpp — exact World mutations and team calls.
3. blue_example.cpp — multi-reason detect/mitigate.
4. pair.cpp — StrategyEntry id, family, narrator.

## Run

```bash
./build/strategy_lab run --tier T2
./build/duel_t2   # if built
```

## Takeaway

No-handle kernel paths trade RPM for driver/device/callback surfaces. Blocklist + callback integrity + ranked policy are the blue stack.
