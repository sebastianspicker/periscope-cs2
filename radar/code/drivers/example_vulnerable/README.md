# gdrv.sys BYOVD — T2 Kernel Mode Memory Access Driver

**Educational prototype** demonstrating **T2 BYOVD (Bring Your Own Vulnerable Driver)** attack techniques against CS2 anti-cheat detection. This driver provides kernel-mode memory access to any process — **including CS2 — without creating a single handle in the radar process's handle table.**

## Device path (single surface)

| Side | Path |
|------|------|
| Kernel device | `\Device\gdrv` |
| DOS symlink | `\DosDevices\gdrv` |
| Usermode open | `\\.\gdrv` |
| SCM service | `gdrv` |

Shared ABI header: `gdrv_abi.h` (IOCTL codes, request layouts, device names).

## Architecture

```
Radar Process (user_test.exe)
  Handle table: \\.\gdrv only  →  DeviceIoControl(IOCTL_*)
        │
        ▼
Kernel Driver (vuln_driver.sys — gdrv.sys pattern)
  PHYS_READ/WRITE   MmMapIoSpace
  VIRT_READ/WRITE   KeStackAttachProcess
  ENTITY_WALK       attach-once entity list walk
  PROCESS_SCAN      ZwQuerySystemInformation (full walk)
  CALLBACK_STRIP    real notify array resolve + query/count/remove
  MODULE_LIST       PEB LDR InLoadOrder walk
        │
        ▼
CS2 / target process (no handle from radar)
```

## IOCTL Interface

| IOCTL Code | Function | Technique |
|------------|----------|-----------|
| `0xC3502000` | PHYS_READ | MmMapIoSpace |
| `0xC3502004` | PHYS_WRITE | MmMapIoSpace |
| `0xC3502008` | VIRT_READ | KeStackAttachProcess |
| `0xC350200C` | VIRT_WRITE | KeStackAttachProcess |
| `0xC3502010` | ENTITY_WALK | KeStackAttachProcess (batch) |
| `0xC3502014` | PROCESS_SCAN | ZwQuerySystemInformation |
| `0xC3502018` | CALLBACK_STRIP | Psp*NotifyRoutine / Cm list resolve |
| `0xC350201C` | MODULE_LIST | PEB→Ldr walk |

## Files

| File | Description |
|------|-------------|
| `gdrv_abi.h` | Shared IOCTL ABI (kernel + usermode + tests) |
| `gdrv_pack.hpp` | Host packing helpers for ABI |
| `vuln_driver.c` | WDM kernel driver (all 8 IOCTLs, real paths) |
| `user_test.cpp` | Usermode client (self-contained) |
| `host_tests.cpp` | Host unit tests for pack/constants |
| `CMakeLists.txt` | CMake build (`BUILD_DRIVER=OFF` default) |
| `Makefile` | nmake/cl build |
| `README.md` | This file |

## Build

### Usermode client + host tests (no WDK)

```cmd
cmake -S . -B build -DBUILD_DRIVER=OFF
cmake --build build --config Release
build\bin\user_test.exe --info
build\bin\user_test.exe --list
build\bin\gdrv_host_tests.exe
```

Or with MSVC directly:

```cmd
cl /EHsc /std:c++20 /I. user_test.cpp /Fe:user_test.exe /link advapi32.lib psapi.lib
cl /EHsc /std:c++20 /I. host_tests.cpp /Fe:gdrv_host_tests.exe
```

### Kernel driver (requires WDK km headers + x64 libs)

```cmd
cmake -S . -B build -DBUILD_DRIVER=ON -DWDK_ROOT="C:\Program Files (x86)\Windows Kits\10"
cmake --build build --config Release
```

## Install & Load (admin + test-signing)

```cmd
copy /Y vuln_driver.sys C:\Windows\System32\drivers\vuln_driver.sys
sc create gdrv type=kernel binPath=C:\Windows\System32\drivers\vuln_driver.sys
sc start gdrv
user_test.exe --scan
sc stop gdrv
sc delete gdrv
```

## Commands

| Command | Driver needed? | Description |
|---------|----------------|-------------|
| `--info` | No | Capabilities + full IOCTL table `0xC3502000`–`0xC350201C` |
| `--list` | No | BYOVD IOCTL list (all 8 gdrv codes) |
| `--scan` | Yes | Full IOCTL smoke suite |
| `--phys 0xADDR` | Yes | Physical memory read |
| `--t2-cs2` | Yes | CS2 PE header via VIRT_READ |
| `--entities` | Yes | T2 entity walk |
| `--processes` | Yes | Kernel process enum |
| `--modules <pid>` | Yes | Module list |
| `--callback [0-3]` | Yes | Query real notify callback counts |

## Educational Purpose Only

Provided **exclusively for security research and educational purposes** inside the anti-cheat-legit-radar lab. Demonstrates why DSE, HVCI, and driver blocklists matter.

## References

- CVE-2020-15368 (gdrv.sys MmMapIoSpace)
- `code/lib/real/kernel/vulnerable_driver.*` (matching IOCTL codes)
- `code/drivers/LESSON.md`
