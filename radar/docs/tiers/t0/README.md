# T0 — Usermode RPM external

**Scar:** process handle with `VM_READ` on the game.  
**Blue:** handle graph (+ co-occurrence).

## Layout

- `red/` — cheat client, weak evasions, RPM backends  
- `blue/` — AC agent, handle monitor, FP policy  
- `strategies/` — external_rpm, internal_inject, handle_minimize, read_throttle, module_integrity  
- `demos/duel` — narrated fight  

## Run

```bash
./build/duel_t0
./build/proto_t0_red && ./build/proto_t0_blue
./build/strategy_lab run 01_external_rpm
```

## Strategies in this tier

| Dir | Red | Blue |
|-----|-----|------|
| `external_rpm` | OpenProcess + RPM | Handle graph |
| `internal_inject` | DLL inject | Module list |
| `handle_minimize` | Brief/least rights | Continuous enum |
| `read_throttle` | Sparse reads | Who reads |
| `module_integrity` | Patch .text | Continuous hash |

See [LESSON.md](LESSON.md).
