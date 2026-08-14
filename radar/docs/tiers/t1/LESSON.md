# T1 Cat-and-Mouse Lesson

Battlefield: Indirect syscall, staging, soft kernel edges. Sim only.

## RED goal

Use the shipped team library `t1_red::SyscallCheat` (and strategy-specific red_example paths) to plant multi-step World scars and achieve an information or control advantage.

## BLUE goal

Use `t1_blue::SyscallAwareMonitor` and pair blue_example sensors for multi-reason detect and/or mitigate. Pass is blue_detected || blue_mitigated || !red_achieved.

## Strategy pairs in this tier

- `67_dse_testsign` — strategies/dse_testsign/LESSON.md
- `45_etw_blind` — strategies/etw_blind/LESSON.md
- `02_indirect_syscall` — strategies/indirect_syscall/LESSON.md
- `32_inmatch_only` — strategies/inmatch_only/LESSON.md
- `08_manual_map_hide` — strategies/manual_map_hide/LESSON.md
- `66_mapper_artifact` — strategies/mapper_artifact/LESSON.md
- `54_module_stomp` — strategies/module_stomp/LESSON.md
- `31_parent_lineage` — strategies/parent_lineage/LESSON.md
- `55_process_hollow` — strategies/process_hollow/LESSON.md
- `91_raw_vs_sendinput` — strategies/raw_vs_sendinput/LESSON.md
- `90_sedebug_priv` — strategies/sedebug_priv/LESSON.md
- `79_speedhack_timescale` — strategies/speedhack_timescale/LESSON.md
- `44_stack_spoof` — strategies/stack_spoof/LESSON.md
- `78_thread_hide_dbg` — strategies/thread_hide_dbg/LESSON.md

## How to read a pair

1. LESSON.md (this folder's strategies/<name>/) — multi-step red scars and blue sensors.
2. red_example.cpp — exact World mutations and team calls.
3. blue_example.cpp — multi-reason detect/mitigate.
4. pair.cpp — StrategyEntry id, family, narrator.

## Run

```bash
./build/strategy_lab run --tier T1
./build/duel_t1   # if built
```

## Takeaway

Syscall soft reduces some usermode API hooks but still leaves process, handle via_syscall_path, staging, and integrity scars.
