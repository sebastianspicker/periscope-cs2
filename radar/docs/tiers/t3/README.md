# T3 — Hypervisor

**Scar:** VBS/HVCI off, personal HV, thin bridge driver.  
**Blue:** ranked trust policy, HV probes, bridge intel, server residual.

## Run

```bash
./build/duel_t3
./build/strategy_lab run 05_hypervisor
./build/strategy_lab run 27_boot_trust
```

## Strategies

| Dir | Topic |
|-----|--------|
| `hypervisor` | Personal HV + bridge |
| `boot_trust` | Disable SB/VBS / early load |

See [LESSON.md](LESSON.md).
