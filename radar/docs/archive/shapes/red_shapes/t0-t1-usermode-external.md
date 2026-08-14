# Red T0–T1: Usermode external radar

**Goal we counter:** Separate process reads game memory and draws (or streams) a top-down radar without injecting into or writing the game.

## Project tree (adversary shape)

```text
red-t0-t1-external-radar/
│
├── CMakeLists.txt | radar.sln | Cargo.toml | *.csproj
├── README.md                    # often claims "read-only / VAC-safe"
│
├── src/
│   ├── main.cpp                 # attach loop, tick rate
│   ├── process/
│   │   ├── attach.cpp           # find PID by name/window
│   │   ├── rpm_backend.cpp      # OpenProcess + ReadProcessMemory   [T0/T1]
│   │   ├── syscall_backend.cpp  # NtReadVirtualMemory via direct syscall [T1]
│   │   └── module_base.cpp      # enum modules / PEB remote read
│   ├── schema/
│   │   ├── offsets.h | offsets.bin.enc
│   │   ├── patterns.cpp         # signature scan fallback
│   │   └── entity_layout.h      # pawn/controller field map
│   ├── game/
│   │   ├── entity_list.cpp      # walk entities
│   │   ├── local_player.cpp
│   │   ├── world_to_radar.cpp   # XZ → 2D map coords
│   │   └── map_id.cpp
│   ├── ui/
│   │   ├── window_main.cpp      # SFML / ImGui / Win32
│   │   ├── radar_draw.cpp
│   │   ├── maps/                # de_*, radar backgrounds
│   │   └── web/                 # optional: websocket server → phone
│   │       ├── server.cpp
│   │       └── static/
│   ├── auth/                    # T1 paid
│   │   ├── license.cpp
│   │   └── hwid.cpp
│   └── protect/
│       ├── string_enc.h
│       ├── api_hash.cpp         # dynamic resolve
│       └── antidebug.cpp
│
├── loader/                      # T1 often split
│   ├── stub.exe                 # tiny on-disk
│   └── stage_payload            # downloaded after login
│
├── updater/
│   └── pull_offsets.cpp         # post-patch schema
│
└── third_party/
    ├── imgui/ | SFML/
    └── (packer project files)
```

## Runtime topology

```text
[radar.exe] --OpenProcess/VM_READ--> [game.exe]
     |
     +--> [radar window] or [localhost:WS] --> [phone browser]
```

## Implementation strategy (for counter-understanding)

| Step | What red does | Blue signal |
|------|----------------|-------------|
| 1 | Resolve game PID | Process name co-occurrence |
| 2 | Open with `VM_READ` (+ query) | **Handle table entry** |
| 3 | Read module base + offsets | Cross-process reads of entity pages |
| 4 | Loop 20–60+ Hz sparse fields | Optional rate heuristic (weak alone) |
| 5 | Draw outside game | Secondary window / port listen |
| 6 | (T1) syscall path | Same handle; skips some hooks |
| 7 | (T1) encrypt offsets + pack | Static YARA weak |

## Variants to expect

- **Phone radar:** PC reader + WebSocket SaaS/localhost.
- **Hide window** hotkey mid-round.
- **Fallback only:** no kernel; pure usermode.

## Blue mapping

→ [`../blue/control-stack.md`](../blue/control-stack.md) § Handle graph, UI co-occurrence  
→ [`../matrices/RED-BLUE-MAP.md`](../matrices/RED-BLUE-MAP.md) rows T0–T1
