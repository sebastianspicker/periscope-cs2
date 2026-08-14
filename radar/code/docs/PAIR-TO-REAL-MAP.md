# Strategy Pair → Real-World Code Mapping

Every educational pair in the catalog has a real-world counterpart in the codebase.
This document maps each pair to its real implementation files.

## T0 — Usermode RPM (lib/real/cs2/)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 01_external_rpm | External RPM | `lib/real/cs2/process.cpp` | `OpenProcess(PROCESS_VM_READ)` |
| 07_internal_inject | Internal Inject | `lib/real/cs2/entities.cpp` | `ReadProcessMemory` entity walk |
| 17_handle_minimize | Handle Minimize | `lib/real/cs2/process.cpp` | brief handle lifetime |
| 18_read_throttle | Read Throttle | `lib/real/cs2/memory.cpp` | `read_virtual()` timing |
| 29_pattern_offset_scan | Pattern Offset Scan | `lib/real/cs2/offsets.cpp` | `scan_pattern()` AOB scan |
| 33_handle_hijack_proxy | Handle Hijack Proxy | `lib/real/cs2/process.cpp` | DuplicateHandle pattern |
| 49_crosshair_helper | Crosshair Helper | `lib/real/gpu/render_pipeline.cpp` | `draw_crosshair()` |
| 50_desktop_dup_capture | Desktop Duplication | `lib/real/gpu/render_pipeline.cpp` | `capture_desktop_frame()` |
| 57_windowless_swapchain | Windowless Swapchain | `lib/real/gpu/render_pipeline.cpp` | D3D11 swapchain |
| 113_self_obfuscate_process | Process Self-Obfuscation | `lib/real/process.cpp` | `enum_processes()` |
| 114_proxy_hijack_reader | Proxy/Hijack Reader | `lib/real/kernel/vulnerable_driver.cpp` | BYOVD KeStackAttachProcess |
| 123_handle_inherit_detect | Handle Inheritance | `lib/real/process.cpp` | `open_process()` CreateProcess |

## T1 — Syscall/Software (lib/real/win/)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 02_indirect_syscall | Indirect Syscall | `lib/real/win/syscall_helper.cpp` | `syscall_4/5/6()` |
| 58_heavens_gate_syscall | Heaven's Gate | `lib/real/win/syscall_helper.cpp` | x86→x64 syscall |
| 59_enhanced_stack_spoof | Stack Spoof | `lib/real/win/syscall_helper.cpp` | SSN resolution |
| 66_dynamic_ssn_resolve | Dynamic SSN | `lib/real/win/syscall_helper.cpp` | SSN extraction from ntdll |
| 67_ntdll_hook_evade | NTDLL Hook Evade | `lib/real/win/api_table.hpp` | `ApiTable::resolve()` |
| 102_vac_handle_enum | VAC Handle Enum | `lib/real/process.cpp` | `enum_processes()` |
| 103_thread_monitor_evade | Thread Monitor Evade | `lib/cs2/diagnostic_system.cpp` | `is_thread_start_suspicious()` |
| 116_etw_dual_provider | ETW Dual Provider | `lib/real/kernel/ioctl_interface.cpp` | DeviceIoControl |
| 117_thread_4kb_signature | Thread 4KB Sig | `lib/cs2/diagnostic_system.cpp` | ThreadContextEntry |
| 118_bsecure_allowed_evade | BSecureAllowed | `lib/cs2/diagnostic_system.cpp` | DllVerificationState |
| 119_vas_walk_evade | VAS Walk Evade | `lib/real/memory.cpp` | `read_virtual()` |
| 121_ret_addr_spoof | Return Address Spoof | `lib/cs2/diagnostic_system.cpp` | ThreadContextEntry |
| 122_norm_hash_evade | Normalized Hash | `lib/real/process.cpp` | module enumeration |

## T2 — Kernel/BYOVD (lib/real/kernel/)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 03_kernel_ioctl | Kernel IOCTL | `lib/real/kernel/ioctl_interface.cpp` | `send_ioctl()` |
| 04_byovd | BYOVD | `lib/real/kernel/vulnerable_driver.cpp` | BYOVD catalog |
| 08_manual_map_hide | Manual Map | `lib/real/kernel/driver_loader.cpp` | `enum_kernel_modules()` |
| 16_callback_strip | Callback Strip | `lib/real/kernel/kernel_memory.cpp` | kernel callbacks |
| 56_iommu_policy | IOMMU Policy | `lib/real/dma/iommu.cpp` | `iommu_enabled()` |
| 60_physmem_direct_read | Physical Direct | `lib/real/kernel/kernel_memory.cpp` | `read_physical()` |
| 61_dkom_token_steal | DKOM Token | `lib/real/kernel/kernel_memory.cpp` | `steal_token()` |
| 62_smm_read_channel | SMM Read | `lib/real/smm/smi.cpp` | `smm_communicate()` |
| 68_acpi_pm_mem_read | ACPI PM Mem | `lib/real/smm/acpi.cpp` | `read_acpi_table()` |
| 69_hypercall_mem_read | Hypercall Mem | `lib/real/vmx/hyperv.cpp` | `hyperv_hypercall()` |
| 104_vmt_integrity | VMT Integrity | `lib/cs2/diagnostic_system.hpp` | VmtEntry |
| 105_diagnostic_telemetry | Diagnostic | `lib/cs2/diagnostic_system.hpp` | DllVerificationState |
| 112_vmt_proxy_evade | VMT Proxy | `lib/cs2/diagnostic_system.hpp` | VmtEntry.usage_count |
| 115_convars_temp_restore | ConVar Restore | `lib/cs2/diagnostic_system.hpp` | ConVarIntegrityResult |
| 120_module_list_hide | Module List Hide | `lib/real/kernel/driver_loader.cpp` | `enum_kernel_modules()` |

## T3 — Hypervisor (lib/real/vmx/)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 05_hypervisor | Hypervisor | `lib/real/vmx/lifecycle.cpp` | `vmxon()`, `vmlaunch()` |
| 27_boot_trust | Boot Trust | `lib/real/vmx/cpuid.cpp` | `vmx_supported()` |
| 54_lag_switch | Lag Switch | `lib/real/net/socket.cpp` | `connect()`, `set_timeout()` |
| 64_scattered_entity_read | Scattered Read | `lib/real/cs2/entities.cpp` | `read_entity_list()` |
| 70_ept_violation_evade | EPT Evade | `lib/real/vmx/ept.cpp` | EPT page table walk |
| 71_vmexit_keylog_capture | VM-Exit Capture | `lib/real/vmx/hyperv.cpp` | `hyperv_hypercall()` |

## T4 — DMA/Hardware (lib/real/dma/ + firmware/)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 06_dma_hardware | DMA Hardware | `lib/real/dma/backend.cpp` | `RealDmaBackend::read()` |
| 56_iommu_policy | IOMMU Policy | `lib/real/dma/iommu.cpp` | `iommu_enabled()` |
| 63_fpga_smart_dma | FPGA DMA | `lib/real/dma/fpga.cpp` | `fpga_scatter_read()` |
| 72_pcie_peer_dma | PCIe Peer DMA | `lib/real/dma/pcie.cpp` | `pcie_peer_read()` |

## Driver Examples

| Directory | What It Models | Real-World |
|-----------|---------------|------------|
| `drivers/example_vulnerable/` | gdrv.sys BYOVD | CVE-2020-15368, IOCTL 0xC3502000 |
| `firmware/example_pcie_dma/` | PCILeech 2.0 B1 FPGA | Xilinx 0x10EE:0x9034, PCIe gen2 x4 |

## Cross-Cutting (GPU, NET, SMM)

| Pair | Title | Real Code | Real API |
|------|-------|-----------|----------|
| 09_overlay_esp | Overlay ESP | `lib/real/gpu/render_pipeline.cpp` | D3D11 overlay |
| 10_web_phone_radar | Web Phone Radar | `lib/real/net/socket.cpp` | TCP socket |
| 11_aim_humanization | Aim Humanization | `lib/real/gpu/render_pipeline.cpp` | draw_radar_frame |
| 14_offset_c2 | Offset C2 | `lib/real/net/c2_client.cpp` | `C2Client::fetch_offsets()` |
| 20_fallback_chain | Fallback Chain | `lib/real/cs2/memory.cpp` | `create_backend_with_fallback()` |
| 22_input_provenance | Input Provenance | `lib/real/smm/tpm.cpp` | `tpm_read_pcr()` |
| 24_interest_mgmt | Interest Mgmt | `lib/real/cs2/entities.cpp` | `Cs2PlayerEntity.to_snapshot()` |
| 25_info_advantage | Info Advantage | `lib/real/cs2/radar.cpp` | `entities_to_radar_snapshot()` |
| 28_network_c2_intel | Network C2 | `lib/real/net/c2_client.cpp` | C2Client |
| 31_composition_radar | Composition Radar | `lib/real/cs2/radar.cpp` | `entities_to_blips()` |
| 34_bomb_round_intel | Bomb Round Intel | `lib/real/cs2/entities.cpp` | read_entity_list |
| 38_sound_esp | Sound ESP | `lib/real/cs2/entities.cpp` | entity positions |
| 52_schema_saas_product | Schema SaaS | `lib/real/net/offset_fetch.cpp` | `fetch_schema_offsets()` |
| 106_obfuscation_crypto | Obfuscation | `lib/real/net/http.cpp` | `https_get()` |
| 107_polymorphic_build | Polymorphic Build | `lib/real/kernel/driver_loader.cpp` | module enumeration |
| 108_anti_re_canary | Anti-RE Canary | `lib/real/cs2/offsets.cpp` | pattern scanning |
| 109_build_watermark | Build Watermark | `lib/real/process.cpp` | module enumeration |
| 110_hwid_spoof | HWID Spoof | `lib/real/smm/efi.cpp` | `efi_get_variable()` |
| 111_staged_loader | Staged Loader | `lib/real/kernel/driver_loader.cpp` | SCM service |
