# T3 — Hypervisor

**Scar:** VBS/HVCI off, personal HV, thin bridge driver.  
**Blue:** ranked trust policy, HV probes, bridge intel, server residual.

## Run

```bash
./anti-cheat/build/dev/duel_t3
./anti-cheat/build/dev/strategy_lab run 05_hypervisor
./anti-cheat/build/dev/strategy_lab run 27_boot_trust
```

## Strategies

| Dir | Topic |
|-----|--------|
| `hypervisor` | Personal HV + bridge |
| `boot_trust` | Disable SB/VBS / early load |

See [LESSON.md](LESSON.md).
