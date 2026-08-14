# Real Anti-Cheat Learnings — What to Implement

Based on three deep-dive documents:
- CS2 Built-in Anti-Cheat (CDllVerificationMonitor)
- VAC External Scanner Architecture
- CS2 Anti-Cheat Deepdive (reverse engineering of Messages 157-163)

## Three Independent Detection Systems

### 1. VAC External Scanner (steam.exe) — T0/T1

**How it works:**
- Handle table enumeration: `NtQuerySystemInformation(SystemHandleInformation)` scans all handles system-wide. Any `PROCESS_VM_READ` handle to cs2.exe from a non-Steam PID is a high-confidence signal.
- VAS walk: `NtQueryVirtualMemory` walks committed pages in cs2.exe at startup (~150+ modules baselined), then continuously polls module descriptors (~136 bytes each).
- Normalized module hashing: CRC32/SHA1 over read-only sections with relocations undone, EAT/IAT zeroed.
- Self-verification: scanner re-reads its own modules to detect tampering.

**Our coverage:**
- `teams/t0_blue/handle_graph_monitor.cpp` — Already simulates handle table scanning.
- `strategies/t0/process_cooccurrence/` — Cross-process co-occurrence.
- **MISSING:** VAS walk simulation, normalized hashing, self-verification loop.

### 2. CS2 In-Process Anti-Tamper (client.dll) — T1/T2

**How it works (CDllVerificationMonitor):**
- Module snapshot: `CModuleListSnapshot::Capture()` enumerates ALL loaded DLLs.
- Thread monitoring: `NtQueryInformationThread(ThreadQuerySetWin32StartAddress)` on `DLL_THREAD_ATTACH` — flags RWX memory, threads in kernel32/kernelbase, no backing module.
- File trust: `BSecureAllowed()` exported from cs2.exe — validates file signatures.
- PE timestamps: Tracks `TimeDateStamp` of client.dll, cs2.exe, kernel32, ntdll, gameoverlayrenderer64.
- ConVar CRC: RB-tree of all ConVars, CRC32 computed in two modes (all, FCVAR_REPLICATED only).
- VMT integrity: 112 global interface VMTs collected + entity VMT rarity analysis.
- CPUID VM detection + `IsDebuggerPresent()`.

### 3. Diagnostic Telemetry (Messages 159, 157, 160) — All Tiers

**Message 159 (DllStatusResponse):** 40+ fields including module list, thread context + 4KB memory dump, PE timestamps, file trust stats, CPUID, app state.

**Message 157 (UtilAction/ConVar):** CRC32 of all ConVars in two modes, detailed ConVar name/value CRC dumps.

**Message 160 (Inventory/VMT):** 112 interface VMTs + entity VMT counts + PE analysis of every loaded module (section count, image size, timestamp, checksum, SHA1).

## What to Implement

### Phase 1: CS2 Diagnostic System Model (BLUE)

Create `lib/cs2/diagnostic_system.hpp/.cpp` — a realistic model of CDllVerificationMonitor:

```cpp
struct CDllVerificationMonitorModel {
  // Module snapshot
  std::vector<ModuleEntry> module_snapshot;
  int total_files_loaded;
  int files_need_trust_check;
  bool b_secure_allowed;
  
  // Thread monitoring
  struct ThreadContext {
    void* start_address;
    void* module_handle;
    uint32_t thread_id;
    uint32_t protection;
    void* return_address;
    void* allocation_base;
    uint8_t signature_buffer[4096];
  };
  std::vector<ThreadContext> suspicious_threads;
  
  // PE timestamps
  uint32_t pe_timestamp_client;
  uint32_t pe_timestamp_cs2;
  uint32_t pe_timestamp_kernel32;
  uint32_t pe_timestamp_ntdll;
  
  // ConVar integrity
  uint32_t convars_crc_all;
  uint32_t convars_crc_replicated;
  
  // VMT integrity
  struct VmtEntry { void* vmt; int usage_count; };
  std::vector<VmtEntry> interface_vmts;
  std::vector<VmtEntry> entity_vmts;
  
  // Detection
  bool debugger_detected;
  bool cpuid_vm_detected;
  bool client_allowed_on_secure;
  
  // Module analysis
  struct ModuleAnalysis {
    std::string name;
    size_t image_size;
    uint32_t timestamp;
    uint32_t checksum;
    uint16_t machine_type;
    uint8_t sha1_hash[20];
    uint32_t section_count;
    uint32_t name_hash;
  };
  std::vector<ModuleAnalysis> analyzed_modules;
};
```

Add diagnostic collection functions that mirror CDllVerificationMonitor::SerializeSystemDiagnostics.

### Phase 2: VAC External Scanner Simulator (BLUE)

Create `teams/t1_blue/vac_scanner_sim.hpp/.cpp`:

- Handle table enum: enumerate handles with PROCESS_VM_READ to cs2.exe
- VAS walk: simulate walking committed memory pages
- Module hashing: CRC32 of normalized read-only sections
- Continuous polling loop with configurable intervals
- Reports findings to the blue detection system

### Phase 3: Anti-Tamper Evasion Strategies (RED)

**Thread monitoring evasion** — strategies that avoid triggering NtQueryInformationThread detection:
- Use legitimate thread start addresses
- Never create RWX memory
- Never start threads in kernel32/kernelbase

**Module trust evasion** — strategies that avoid detection by BSecureAllowed:
- Manual mapping (no module entry in loaded list)
- Signed driver loading (legitimate module trust path)
- Module stomping (legitimate module overwritten)

**VMT integrity evasion** — strategies that avoid VMT detection:
- Use virtual function dispatch without modifying vtables
- Direct function calls instead of virtual dispatch
- Proxy pattern for interface interception

**ConVar CRC evasion** — strategies that avoid ConVar CRC mismatches:
- Temporary ConVar modifications with restoration
- Memory breakpoints to intercept CRC reads

**PE timestamp consistency** — strategies that preserve expected PE timestamps:
- Timestamp cloning from legitimate modules
- File timestamp synchronization

### Phase 4: New Strategy Pairs

Based on the real AC mechanisms, these new pairs should be added:

| Strategy | Tier | Family | Red | Blue |
|----------|------|--------|-----|------|
| thread_monitor_evade | T1 | Evasion | Thread using legitimate start address | NtQueryInformationThread monitor |
| module_trust_evade | T1 | Evasion | Manual mapped module | BSecureAllowed trust check |
| vmt_integrity | T2 | Detection | Proxy VMT interception | Interface VMT CRC |
| convars_crc_evade | T2 | Evasion | Temporary ConVar modify | ConVar CRC check |
| pe_timestamp_spoof | T2 | Evasion | Clone legitimate PE timestamp | PE timestamp verification |
| diagnostic_telemetry | T1 | Detection | External read only | Message 159 field collection |
| vac_handle_enum | T0 | Detection | OpenProcess handle | Handle table enumeration |
| vas_walk_evade | T1 | Evasion | No anomalous memory regions | VAS walk & module baseline |

## Implementation Priority

1. **HIGH** — CS2 Diagnostic System Model (BLUE foundation for T1-T2 detection)
2. **HIGH** — VAC Handle Enumeration Strategy (completes the T0 detection story)
3. **MEDIUM** — Thread Monitoring Strategy + Evasion (key T1 mechanic)
4. **MEDIUM** — VMT Integrity Strategy (key T2 mechanic)
5. **LOW** — ConVar, PE timestamp, VAS walk strategies (advanced topics)

See also:
- `lib/cs2/diagnostic_sensors.cpp` — existing CS2 diagnostic simulation
- `teams/t0_blue/handle_graph_monitor.cpp` — existing handle enumeration
- `strategies/t2/callback_strip/` — existing kernel-level evasion
