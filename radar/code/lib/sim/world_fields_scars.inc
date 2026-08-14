// world_fields_scars.inc — wave/scar residual members (included inside struct World).
  // T3: SMM (System Management Mode) read channel — red plants an SMM
  // handler that reads game memory at SMI request. Bypasses all HV-level
  // protections because SMM runs at the highest privilege level.
  bool smm_read_channel_planted = false;
  // Number of successful SMM memory reads.
  int smm_read_ops = 0;

  // T3: TPM measurement spoofing — red manipulates TPM PCR values to
  // present fake attestation measurements, bypassing remote attestation.
  bool tpm_measurement_spoofed = false;
  // Which PCR bank was spoofed (PCR 0-7 for firmware/BIOS).
  int spoofed_pcr_bank = 0;

  // T3: EPT violation side-channel evasion — red monitors EPT violation
  // timing to detect when AC is scanning game memory pages. Evades reads
  // only during AC scan windows, minimizing exposure. Uses INVEPT to
  // flush EPT caches and force VM exits on AC access.
  bool ept_sidechannel_active = false;
  // Number of AC scan windows evaded via EPT side channel.
  int ept_ac_scan_evasions = 0;
  // Whether INVEPT was used to flush EPT cached mappings.
  bool invept_tlb_flush_used = false;

  // T3: VM-exit keylog capture — red intercepts VM exits caused by
  // external interrupts (including keyboard IRQs) to capture keystrokes.
  // Works even without any game process modifications because VM exits
  // are hypervisor-level events invisible to the guest.
  bool vmexit_keylog_active = false;
  // Number of keystrokes captured via VM-exit interception.
  int vmexit_keys_captured = 0;

  // T4: FPGA smart DMA (PCILeech pattern) — FPGA-based DMA with
  // automated scatter-gather entity scanning. Unlike basic dma_read,
  // this does per-page scatter reads that avoid bulk DMA detection.
  bool fpga_smart_dma_active = false;
  // Number of automated scatter-gather read operations.
  int fpga_scatter_reads = 0;
  // Whether FPGA does hidden re-scans (not burst).
  bool fpga_hidden_rescan = false;

  // T4: IOMMU bypass via PCIe config — manipulates PCIe configuration
  // space ATS/ACS to route DMA around IOMMU. Allows dma_read even when
  // IOMMU is nominally enabled.
  bool iommu_bypass_active = false;
  // Whether IOMMU bypass was confirmed working.
  bool iommu_bypass_confirmed = false;

  // ── UEFI/DMAR/IOMMU residual model (uefi_dma_bypass) ──
  IommuState iommu_state = IommuState::Enabled;
  bool iommu_initially_enabled = true;
  bool iommu_disabled = false;
  bool acs_cleared = false;
  bool dma_enabled = false;
  int dmar_drhd_count = 0;

  // ── Measured-boot / TPM event-log residual model (uefi_txt_measured_boot) ──
  bool pcr_value_tampered = false;
  std::vector<TpmEventLogEntry> tpm_event_log_entries;

  // T4: PCIe peer-to-peer DMA — red uses two PCIe devices to transfer
  // data directly without involving host memory or CPU. One device reads
  // target physical memory and the other exfiltrates it. Bypasses IOMMU
  // because peer-to-peer transactions may not be translated.
  bool pcie_peer_dma_active = false;
  // Number of peer-to-peer DMA transactions performed.
  int pcie_peer_transactions = 0;
  // Whether peer-to-peer bypassed IOMMU successfully.
  bool pcie_peer_bypassed_iommu = false;

  // ── Wave 11: Deep red team evasion scars (round 2 depth) ──

  // T0: Process hollowing — spawn a legitimate process (e.g. svchost.exe), hollow it,
  // map cheat code into its address space, and transfer execution. Harder to detect
  // than manual_map because the original image name is clean.
  bool process_hollowing_active = false;
  // PID of the hollowed process.
  std::uint32_t hollowed_pid = 0;
  // The original image path before hollowing.
  std::string hollowed_original_image;
  // APC injection active — red uses QueueUserAPC to inject code into an existing
  // game thread. Leaves no CreateRemoteThread scar.
  bool apc_injection_active = false;
  // Number of APC injections performed.
  int apc_injection_count = 0;
  // Named pipe / LPC IPC — red uses named pipes or LPC ports for inter-process
  // entity relay instead of shared sections. Harder for blue to attribute.
  bool named_pipe_ipc_active = false;
  // Number of named pipe messages sent for entity relay.
  int named_pipe_message_count = 0;
  // Registry-persisted configuration — red stores cheat configuration in the
  // Windows registry (e.g. HKCU\Software\Microsoft\Windows\CurrentVersion\Run).
  bool registry_persisted_config = false;
  // Registry key path used for persistence.
  std::string registry_key_path;
  // Binary padding / size manipulation — red pads the cheat binary with null bytes
  // or appends junk data to alter its hash. Evades hash-based blocklists.
  bool binary_padding_applied = false;
  // New size after padding (bytes).
  std::size_t padded_binary_size = 0;

  // T1: ROP chain syscall — red constructs a ROP chain to execute syscalls without
  // ever calling ntdll functions directly. Defeats ntdll hooking even if the
  // clean-copy approach fails.
  bool rop_chain_syscall_active = false;
  // Number of ROP gadgets used in the chain.
  int rop_gadget_count = 0;
  // Hardware breakpoint evasion — red clears/monitors debug registers (DR0-DR7)
  // to detect and evade hardware breakpoint-based detection.
  bool hw_breakpoint_evasion_active = false;
  // Whether debug registers were cleared.
  bool debug_registers_cleared = false;
  // VEH control-flow patched — red patches the VEH chain to bypass
  // control-flow guard (CFG) checks. /GUARD:CF bypass.
  bool veh_cf_patched = false;
  // Number of VEH handlers modified.
  int veh_handlers_modified = 0;
  // Dynamic import resolution — red resolves all imports at runtime via
  // LoadLibrary/GetProcAddress manually. PE import table is empty.
  bool dynamic_import_resolution = false;
  // Number of APIs resolved dynamically.
  int dynamic_import_count = 0;

  // T2: HAL heap exploitation — red uses the HAL heap (HalPrivateDispatchTable or
  // similar) for kernel-mode memory allocation. Evades pool scanner detection.
  bool hal_heap_exploit_active = false;
  // Whether HAL heap was used for payload execution.
  bool hal_heap_payload_executed = false;
  // Kernel NULL pointer dereference — red allocates kernel memory at NULL,
  // then triggers a NULL deref to gain execution with SYSTEM privileges.
  bool null_ptr_deref_exploit_active = false;
  // DPC (Deferred Procedure Call) execution — red queues a DPC to execute
  // kernel-mode code at DISPATCH_LEVEL. Evades thread-level detection.
  bool dpc_execution_active = false;
  // Number of DPCs queued for execution.
  int dpc_queue_count = 0;
  // Kernel module shadowing — red loads a driver then unlinks it from
  // PsLoadedModuleList. Evades module enumeration.
  bool module_shadowing_active = false;
  // The name of the shadowed driver.
  std::string shadowed_driver_name;

  // T3: EPT nested paging hide — red manipulates EPT nested page tables beyond
  // basic EPT page hiding. Creates shadow EPT structures that blue HV probes
  // cannot easily enumerate.
  bool ept_nested_hide_active = false;
  // Number of shadow EPT page table levels created.
  int ept_shadow_levels = 0;
  // MSR hooking — red hooks model-specific registers (IA32_LSTAR for syscall,
  // IA32_SYSENTER_EIP for sysenter) to intercept OS operations invisibly.
  bool msr_hooking_active = false;
  // Which MSRs were hooked: "lstar", "sysenter", "star", "cstar".
  std::string hooked_msr_list;
  // Guest OS time dilation — red manipulates the guest's perception of time
  // (TSC, HPET, ACPI timer) to evade timing-based detections.
  bool guest_time_dilation_active = false;
  // Time dilation factor (1.0 = normal, 0.5 = half speed perception).
  double time_dilation_factor = 1.0;
  // Hyper-V enlightenment interface exploitation — red uses Hyper-V
  // enlightenment interfaces (e.g. hypercalls via hvcall instruction) to
  // read protected process memory even under VBS.
  bool hyperv_enlightenment_exploit = false;
  // The enlightenment number used.
  int hyperv_enlightenment_call = 0;

  // T4: Thunderbolt DMA — red uses Thunderbolt PCIe hotplug to attach a DMA
  // device without opening the system chassis. More accessible than internal PCIe.
  bool thunderbolt_dma_active = false;
  // Whether Thunderbolt security was bypassed (e.g. SecurityLevel=0).
  bool thunderbolt_security_bypassed = false;
  // PCIe BAR MMIO manipulation — red reconfigures PCIe BAR (Base Address
  // Register) to map physical memory into MMIO space for direct reads.
  bool pcie_bar_mmio_active = false;
  // Number of PCIe BARs remapped.
  int pcie_bar_remap_count = 0;
  // ACPI DSDT override — red overrides the ACPI DSDT (Differentiated System
  // Description Table) to persist SMM code across reboots.
  bool acpi_dsdt_override_active = false;
  // USB Device Firmware Upgrade (DFU) class DMA — red uses a USB device's
  // DFU interface to perform DMA into host memory.
  bool usb_dfu_dma_active = false;
  // The USB device VID:PID used.
  std::string usb_dfu_device_id;

  // ── Cross-cutting red team depth (round 2) ──

  // Adaptive strategy — red probes blue sensors and dynamically selects which
  // evasion techniques to apply. Improves evasion success probability.
  bool adaptive_strategy_active = false;
  // Number of blue sensors profiled.
  int blue_sensors_profiled = 0;
  // Per-technique evasion probability against current blue sensor set (0.0-1.0).
  double current_evasion_probability = 0.0;
  // Multi-tick operational security — red varies its behaviour patterns across
  // World ticks (read order, timing, IPC method). Defeats temporal correlation.
  bool multi_tick_opsec_active = false;
  // Current opsec pattern variant (changes each tick).
  int opsec_pattern_variant = 0;
  // Number of distinct opsec patterns available.
  int opsec_pattern_count = 0;
  // C2 domain fronting — red routes C2 traffic through a legitimate CDN
  // (Cloudflare, Akamai) to hide the true C2 endpoint.
  bool c2_domain_fronting_active = false;
  // The CDN hostname used for fronting.
  std::string c2_fronting_host;
  // C2 fallback chain — red maintains multiple C2 endpoints with ordered
  // fallback. If one is blocked, the next is tried.
  bool c2_fallback_chain_active = false;
  // Number of C2 endpoints in the fallback chain.
  int c2_fallback_endpoint_count = 0;
  // Whether C2 communication uses out-of-band channels (e.g. DNS tunneling).
  bool c2_out_of_band_active = false;
  // The out-of-band channel type: "dns", "icmp", "http", "websocket".
  std::string c2_oob_channel;
  // Stealth exfiltration — red exfiltrates data through low-and-slow
  // techniques (DNS queries, HTTP timings) instead of bulk uploads.
  bool stealth_exfiltration_active = false;
  // Bytes exfiltrated via stealth channel.
  std::uint64_t stealth_exfiltrated_bytes = 0;

  // ── Wave 12: CS2 built-in anti-cheat diagnostic system (real CS2 AC) ──

  // CDllVerificationMonitor state — simulates CS2's built-in diagnostic
  // data collection with 40+ fields, module scanning, VMT integrity,
  // exception monitoring, and counter-strafe analysis.
  cs2::DiagnosticTelemetryState diagnostic_state;

  // Whether the diagnostic system is active (collecting data).
  bool diagnostic_system_active = false;

  // Diagnostic message flags: which checks have been performed.
  bool diag_module_snapshot_taken = false;
  bool diag_pe_timestamps_sent = false;
  bool diag_thread_captured = false;
  bool diag_convar_checked = false;
  bool diag_vmt_collected = false;
  bool diag_memory_dump_performed = false;
  bool diag_exception_handler_active = false;
  bool diag_counter_strafe_monitored = false;
  bool diag_debugger_checked = false;
  bool diag_cpuid_vm_checked = false;
  bool diag_file_integrity_checked = false;

  // Number of diagnostic responses sent to server.
  int diagnostic_response_count = 0;

  // Whether the client is allowed on secure servers (from BSecureAllowed).
  bool client_allowed_on_secure_servers = true;

  // Silent exclusion (BSecureAllowed mechanism).
  bool client_allowed_on_secure = true;
  bool silent_exclusion_active = false;
  std::uint64_t exclusion_tick = 0;
  int evidence_count = 0;
  int evidence_threshold = 10;

  // Simulated PE timestamps for key CS2 modules.
  std::uint32_t pe_timestamp_client_dll = 0x66800000;
  std::uint32_t pe_timestamp_cs2_exe = 0x667F0000;
  std::uint32_t pe_timestamp_kernel32 = 0x66000000;
  std::uint32_t pe_timestamp_ntdll = 0x65FF0000;
  std::uint32_t pe_timestamp_gameoverlay = 0x667E0000;

  // Whether this game session's diagnostic data has been collected.
  bool diag_session_recorded = false;

  // Simulated counter-strafe analysis.
  int cs_perfect_frames = 0;
  int cs_total_actions = 0;
  double cs_average_tick_delta = 0.0;

  // ── Wave 14: Self-patching and exception analysis ──
  // Simulates CS2's VEH self-healing mechanism where game writes to .text
  // via access violations that the VEH handler silently fixes.
  bool game_self_patching_active = false;    // Game code writes to .text via AV
  int game_self_patch_count = 0;             // Number of self-patches performed
  bool red_hiding_behind_self_patch = false; // Red exploits same AV mechanism

  // ── Wave 13: Overlay hijack simulation scars ──
  // These model observable presentation ownership only; no OS or GPU calls occur.
  bool gameoverlay_loaded = false;
  bool steam_original_trampolines_saved = false;
  bool steam_trampoline_hooks_written = false;
  bool steam_present_hooked = false;
  bool steam_resize_hooked = false;
  bool swapchain_pinned = false;
  int pin_wait_frames = 0;
  bool vtable_present_hook_fallback = false;
  bool steam_critical_section_active = false;
  bool window_hijacked = false;
  std::uint32_t hwnd_owner_pid = 0;
  std::uint32_t presenter_pid = 0;
  bool cross_process_hijack = false;

  // Simulated exception tracking.
  int veh_exception_count = 0;
  std::uint64_t last_exception_address = 0;
  std::uint32_t last_exception_code = 0;

  // ── Wave 15: New strategy pair scars (IDs 124-147) ──

  // 124: handle_hijack_proxy_donor — Donor hijack via NtQSI + DuplicateHandle
  bool donor_hijack_active = false;
  std::uint32_t donor_hijack_donor_pid = 0;
  std::uint32_t donor_hijack_consumer_pid = 0;
  bool donor_handle_duplicated = false;

  // 125: wda_exclude_capture — SetWindowDisplayAffinity EXCLUDEFROMCAPTURE
  bool wda_exclude_updated = false;
  int wda_exclude_count = 0;

  // 126: dxgi_output_duplication — IDXGIOutputDuplication output composite
  bool dxgi_output_dup_active = false;
  int dxgi_output_frames_acquired = 0;
  bool dxgi_output_composite_rendered = false;

  // 127: hud_radar_parsing — CCSGO_HudRadar BST walk + snapshot
  bool hud_radar_snapshot_taken = false;
  int hud_radar_entity_count = 0;
  int hud_radar_bst_nodes_walked = 0;

  // 128: cvar_walk_resolve — 4-tier CVar resolution cascade
  bool cvar_walk_resolved = false;
  int cvar_walk_tier_reached = 0;
  int cvar_walk_entries_found = 0;

  // 130: batch_read_obfuscation — Batch reads with Fisher-Yates shuffle + jitter
  bool batch_read_obfuscated = false;
  int batch_read_count = 0;
  bool batch_read_shuffled = false;
  bool batch_read_jittered = false;

  // 131: temporal_phase_evasion — 4-phase temporal jitter engine
  bool temporal_phase_active = false;
  int temporal_phase_index = 0;
  int temporal_phase_transitions = 0;
  int temporal_phase_ticks_in_phase = 0;

  // 132: decoy_render_ml_evasion — ML behavioral confusion via decoy render
  bool decoy_render_active = false;
  int decoy_frame_count = 0;
  bool ml_confusion_active = false;
  int ml_decoy_patterns_generated = 0;

  // 134: stack_spoof_syscall — Return address spoofing for syscalls
  bool stack_spoof_syscall_active = false;
  int stack_spoof_call_depth = 0;
  bool stack_spoof_ret_addr_forged = false;

  // 135: callback_shadow_kernel — Kernel callback strip for BYOVD
  bool callback_kernel_shadow_active = false;
  std::size_t callback_kernel_process_notify_restored = 0;
  std::size_t callback_kernel_image_notify_restored = 0;
  bool callback_kernel_stripped_during_ioctl = false;

  // 136: ept_memory_hiding — EPT-based physical memory hiding
  bool ept_memory_hiding_active = false;
  int ept_hidden_page_count = 0;
  bool ept_invept_used = false;

  // 137: dma_page_table_walk — Physical memory page table walk via DMA
  bool dma_page_table_walked = false;
  std::uint64_t dma_page_table_base_pfn = 0;
  int dma_page_entries_found = 0;
  int dma_page_levels_walked = 0;

  // 138: shellcode_inject_donor — Obfuscated shellcode injection into donor
  bool shellcode_donor_active = false;
  std::uint32_t shellcode_donor_pid = 0;
  bool shellcode_obfuscated = false;
  bool shellcode_xor_key_applied = false;

  // 139: helper_ticket_protocol — Shared memory helper ticket protocol
  bool helper_ticket_active = false;
  std::uint32_t helper_ticket_id = 0;
  bool helper_ticket_shared_memory_created = false;
  bool helper_ticket_claimed = false;

  // 140: runtime_health_ladder — 3-level healing ladder
  int health_ladder_level = 0;
  int health_ladder_transition_count = 0;
  int health_ladder_ticks_at_level = 0;
  bool health_ladder_self_heal_armed = false;

  // 141: accept_readiness_gate — 7-condition session readiness gate
  bool accept_gate_open = false;
  int accept_conditions_met = 0;
  int accept_conditions_total = 7;
  bool accept_session_ready = false;

  // 142: hw_monitor_disguise — RTSS/Afterburner decoy OSD display
  bool hw_monitor_disguise_active = false;
  bool hw_monitor_decoy_osd_active = false;
  bool hw_monitor_rtss_hijack = false;

  // 143: forensic_cleanup_exit — Prefetch/Registry/Recent cleanup on exit
  bool forensic_cleanup_active = false;
  int forensic_cleanup_steps_completed = 0;
  bool forensic_prefetch_cleared = false;
  bool forensic_recent_cleared = false;

  // 144: peb_spoof_hide — PEB BeingDebugged/NtGlobalFlag clearing
  bool peb_spoof_active = false;
  bool peb_being_debugged_cleared = false;
  bool peb_nt_global_flag_cleared = false;

  // 145: page_oscillation_mem — Code page oscillation for VAS walk
  bool page_oscillation_active = false;
  int page_oscillation_cycles = 0;
  bool code_page_flipped_to_rw = false;
  bool code_page_flipped_to_rx = false;

  // 146: normalized_hash_evade — VAC normalized PE hash evasion
  bool normalized_hash_evade_active = false;
  bool pe_hash_normalized = false;
  int pe_section_entries_altered = 0;

  // 147: vas_walk_evade_phase — VAS walk phase detection + page hiding
  bool vas_walk_evade_phase_active = false;
  int vas_walk_evasion_count = 0;
  bool vas_page_hidden = false;
  bool vas_region_reshuffled = false;

  // ── Remaining Wave 6–12 IDs (added later, placed here for ordering) ──

  // 116: etw_dual_provider — ETW dual provider counter-blinding
  bool etw_secondary_active = false;

  // 117: thread_4kb_signature — Thread 4KB signature buffer evasion
  bool thread_start_in_legitimate_module = false;
  std::vector<std::uint8_t> thread_start_4kb;
  std::string thread_memory_protection;

  // 118: bsecure_allowed_evade — BSecureAllowed trust chain evasion
  bool module_loaded = false;
  bool module_is_stomped = false;
  bool module_is_signed = false;
  std::uint32_t module_signature_date = 0;
  std::uint32_t module_timestamp = 0;
  std::size_t module_size = 0;
  std::size_t expected_module_size = 0;

  // 121: ret_addr_spoof — Return address chain spoofing
  bool thread_start_clean = false;
  bool return_address_chain_clean = false;
  std::vector<std::uint64_t> thread_return_chain;

  // ── Wave 13: Overlay hijacking and Steam overlay hook techniques ──

  // Steam Overlay GameOverlayRenderer64.dll pointer verification state.
  // Base address of GameOverlayRenderer64.dll (for offset calculations).
  std::uint64_t gameoverlay_base = 0;
  // Offset to Steam's Present trampoline pointer in GameOverlayRenderer64.dll.
  std::uint32_t steam_present_ptr_offset = 0x162200;
  // Offset to Steam's ResizeBuffers trampoline pointer.
  std::uint32_t steam_resize_buffers_offset = 0x162208;
  // Whether red successfully overwrote Steam's ResizeBuffers pointer.
  bool steam_resize_buffers_hooked = false;
  // Original Steam trampoline pointers (saved for unhook).
  std::uint64_t original_steam_present_ptr = 0;
  std::uint64_t original_steam_resize_buffers_ptr = 0;
  // Red's hook function address (simulated).
  std::uint64_t hook_present_fn = 0;
  std::uint64_t hook_resize_buffers_fn = 0;

  // DXGI swap chain state for hijack detection.
  // Number of swap chains found in the process.
  int swap_chain_count = 0;
  // OutputWindow HWND from GetDesc.
  std::uint64_t swap_chain_output_window = 0;
  // PID that owns the OutputWindow HWND.
  std::uint32_t output_window_owner_pid = 0;
  // Whether a cross-process mismatch was detected (presenter != hwnd owner).
  bool cross_process_swapchain = false;

  // Overlay detection state (blue).
  bool overlay_hijack_checked = false;
  bool overlay_hijack_detected = false;
  bool steam_hook_checked = false;
  bool steam_hook_detected = false;

