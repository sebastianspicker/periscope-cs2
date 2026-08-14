# T2 — Kernel / BYOVD

Scar: no game handle; driver + device (+ optional callback strip).  
Blue: image load, BYOVD blocklist, device watch, callback integrity.

## Run

```bash
./build/duel_t2
./build/strategy_lab run 03_kernel_ioctl
./build/strategy_lab run 04_byovd
./build/strategy_lab run 33_driver_allowlist
./build/strategy_lab run 34_scm_service
./build/strategy_lab run 35_callback_shadow
```

## Strategies

| Dir | Topic |
|-----|--------|
| `kernel_ioctl` | Custom memrw driver |
| `byovd` | Signed vulnerable driver |
| `callback_strip` | Notify chain tamper |
| `driver_allowlist` | Ranked strict sha allowlist |
| `scm_service` | SCM kernel-driver service scar |
| `callback_shadow` | Clean façade over degraded notifies |

See [LESSON.md](LESSON.md).
