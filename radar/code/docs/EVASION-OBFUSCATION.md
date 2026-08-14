# Evasion & Obfuscation — Real AC Techniques vs. Project Coverage

Based on CS2 anti-tamper / VAC deep-dive documents and the project audit.

## Detection Vectors and Their Evasions

### T0: Handle Table + RPM Telemetry (VAC scanner)

| Detection | Real AC Mechanism | Evasion Technique | Project Coverage |
|-----------|------------------|-------------------|------------------|
| Handle with VM_READ | `NtQuerySystemInformation(SystemHandleInformation)` | **Proxy/hijack reader** — radar never opens handle | `114_proxy_hijack_reader` NEW |
| Handle from unknown PID | Cross-ref handle PID to known processes | **Disguise** as RTSS/Discord/Steam | `113_self_obfuscate_process` NEW |
| RPM volume/cadence | Read count + timing patterns | **Read throttling** with jitter | `read_throttle` (existing) |
| Handle duration | Long-lived handle is higher signal | **Brief handle** + reopen | `handle_minimize` (existing) |
| Process co-occurrence | Two processes same session, one reading | **Process proxy** split | `proxy_hijack_reader` covers this |

### T1: Thread Monitoring + Module Trust (CS2 client.dll)

| Detection | Real AC Mechanism | Evasion Technique | Project Coverage |
|-----------|------------------|-------------------|------------------|
| RWX thread memory | `MonitorThreadContext` + `VirtualQuery` | **Legitimate memory protection** | `103_thread_monitor_evade` NEW |
| Thread in kernel32| `GetModuleHandleEx(FROM_ADDRESS)` | **Start address in game DLL** | `thread_monitor_evade` (red passes) |
| No backing module | Cross-ref start address to module list | **Module stomp** (overwrite legit module) | `module_stomp` (existing) |
| Unsigned module loaded | `BSecureAllowed()` trust check | **Manual mapping** (no module entry) | `manual_map_hide` (existing) |
| PE timestamp mismatch | `GetPETimestamp` on critical DLLs | **Timestamp clone** from legit module | Missing — needs pair |
| Normalized hash mismatch | CRC32/SHA1 over read-only sections | **Polymorphic build** (changes hash each build) | `107_polymorphic_build` NEW |
| String/import artifacts | Static analysis of cheat binary | **Obfuscation** (XOR strings, dynamic imports) | `106_obfuscation_crypto` NEW |
| ETW events | `EtwEventWrite` syscall | **ETW blind** (filter provider) | `etw_blind` (existing) |
| Stack trace capture | `RtlWalkFrameChain` / stack unwind | **Stack spoof** (fake return addresses) | `enhanced_stack_spoof`, `stack_spoof` (existing) |
| 4KB signature buffer | `memcpy(signature_buffer, thread_start, 4096)` | **Clean thread entry point** | Thread evasion ensures first page not suspicious |

### T2: VMT Integrity + ConVar CRC + Kernel (CS2 client.dll)

| Detection | Real AC Mechanism | Evasion Technique | Project Coverage |
|-----------|------------------|-------------------|------------------|
| VMT offset changed | `CollectInterfacesData` — 112 VMTs CRC'd | **VMT proxy** (not hook, new object) | `112_vmt_proxy_evade` NEW |
| Entity VMT count=1 | Entity VMT rarity analysis | **Proxy object with multiple users** | VMT proxy pair covers this |
| ConVar CRC mismatch | `ComputeConVarChecksum` (Message 157) | **Temporary modify + restore** | Missing — needs pair |
| Kernel callback changed | Callback count baseline check | **Callback shadow** (spoof count) | `callback_shadow` (existing) |
| Driver load detected | SCM → driver list → hash check | **BYOVD** (use already-loaded vulnerable driver) | `byovd` (existing) |

### T3-T4: Hypervisor + DMA + Firmware

| Detection | Real AC Mechanism | Evasion Technique | Project Coverage |
|-----------|------------------|-------------------|------------------|
| VM detected (CPUID) | CPUID leaf check in telemetry | **Nested virtualization** / timing spoof | `timing_spoof`, `nested_hv` (existing) |
| EPT page table visible | Scan physical memory for VMCS/EPT | **EPT hide** | `ept_hide_ac` (existing) |
| DMA visible | PCIe device enumeration | **FPGA DMA** | `fpga_smart_dma` (existing) |

### Cross-Cutting: Anti-Analysis + Behavioral

| Detection | Real AC Mechanism | Evasion Technique | Project Coverage |
|-----------|------------------|-------------------|------------------|
| Debugger detected | `IsDebuggerPresent()` / NtGlobalFlag | **PEB hide** / anti-debug | `anti_re_canary` covers this |
| VM sandbox detected | CPUID, MAC, disk patterns | **Sandbox detection** + canary trigger | `108_anti_re_canary` NEW |
| Binary watermark | Build-specific unique markers | **Watermark removal** | `109_build_watermark` NEW |
| HWID ban evasion | Ban by MAC/disk/volume/TPM | **HWID spoof** | `110_hwid_spoof` NEW |
| Staged analysis | Detect loader → cheat separation | **Staged delivery** | `111_staged_loader` NEW |
| Behavioral correlation | VACnet analysis of movement/aim | **Behavioral decoupling** | `behavioral_decoupler` (existing) |

## Gap Analysis

### Still Missing (Not Yet Implemented)

| Gap | Real AC Target | Priority | Recommended Strategy Name |
|-----|---------------|----------|--------------------------|
| ConVar temporary restore | Message 157 CRC detection | MEDIUM | `convars_temp_restore` (T2 Evasion) |
| PE timestamp cloning | BSecureAllowed + PE timestamp check | MEDIUM | `pe_timestamp_clone` (T2 Evasion) |
| Normalized hash evasion | VAC module hashing | HIGH | `norm_hash_evade` (T1 Evasion) |
| VAS walk evasion | VAC startup VAS baseline | MEDIUM | `vas_walk_evade` (T1 Evasion) |
| Self verification | VAC re-reads own modules | LOW | Not applicable (educational) |
| IPC channel decorrelation | Detect pipe/socket between proxy and radar | LOW | Already modeled in proxy_hijack |

## Architecture

```
Evasions (red)                 Detections (blue)
─────────────────              ──────────────────
proxy_hijack_reader     ──→    handle_graph_monitor (blind spot)
self_obfuscate_process  ──→    vac handle scan
thread_monitor_evade    ──→    NtQueryInformationThread
vmt_proxy_evade         ──→    CollectInterfacesData (Message 160)
obfuscation_crypto      ──→    static analysis
polymorphic_build       ──→    normalized hash detection
anti_re_canary          ──→    debugger detection / sandbox
build_watermark         ──→    watermark correlation
hwid_spoof              ──→    HWID consistency check
staged_loader           ──→    process injection detection
```

## Build

After all changes, the strategy catalog has 110+ registered entries.
Run `./build/strategy_lab list` to see evasion pairs with `[real-capable]` tags.
