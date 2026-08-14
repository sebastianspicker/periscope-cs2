# T1 — Syscall / soft external

**Scar:** still a handle; path bypasses usermode API hooks.  
**Blue:** handle truth (not ntdll hooks) + staging signals.

## Run

```bash
./build/duel_t1
./build/strategy_lab run 02_indirect_syscall
```

## Strategies

| Dir | Topic |
|-----|--------|
| `indirect_syscall` | Syscall path vs hooks |
| `manual_map_hide` | PEB unlink vs region scan |

See [LESSON.md](LESSON.md).
