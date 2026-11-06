// world_fields_core.inc — core sim::World data members (included inside struct World).
  std::uint32_t next_pid = 1000;
  std::unordered_map<std::uint32_t, Process> processes;
  std::vector<Handle> handles;
  // System-wide handle table view (lifecycle + acquisition model metadata).
  // Populated by open_process / MemoryReader / DonorDiscovery; blue queries it.
  HandleTable handle_table;
  // Active acquisition scar (set by MemoryReader factory / activate_acquisition).
  ac::MemoryAcquisitionModel active_memory_model =
      ac::MemoryAcquisitionModel::DirectRpm;
  ac::HandleAcquisitionModel active_handle_model =
      ac::HandleAcquisitionModel::None;
  // ── Handle-inheritance model (handle_inherit_detect) ──
  std::vector<Handle> inherited_handles;  // handles inherited from parent (via_proxy)
  std::vector<Driver> drivers;
  std::vector<Device> devices;
  std::vector<OverlayWindow> overlays;
  std::vector<InputEvent> inputs;
  std::vector<Account> accounts;
  std::vector<NetFlow> net;
  std::vector<ServiceEvent> services;
  HostTrust trust{};
  std::vector<std::string> log;

  // ── Narrative + review flags (blue mitigate logging) ──
  std::vector<std::string> notes;  // parallel to `log`; note() may append here too
  std::uint32_t flags = 0;         // review/ban bitmask

  // Callback model for strip / shadow lessons
  std::size_t process_notify = 4;
  std::size_t image_notify = 3;
  bool ac_callback_present = true;
  bool callback_shadow_active = false;  // red restores lists only during probe
  std::size_t process_notify_true = 4;  // ground truth when shadowing
  std::size_t image_notify_true = 3;
  bool ac_callback_true = true;

  // Session / ranked policy
  bool match_active = false;
  bool ranked_strict_driver_allowlist = false;
  std::vector<std::string> driver_allowlist_sha;  // when strict
  bool ranked_access_denied = false;  // blue session deny (BYOVD/policy mitigate)
  bool byovd_policy_block = false;    // deny known-bad driver IOCTL after block

  // Object callbacks (handle create) — distinct from process notify
  std::size_t object_callbacks = 3;
  bool object_callbacks_present = true;
  std::size_t object_callbacks_true = 3;
  bool object_callbacks_true_present = true;

  // ETW / stack / section / RE
  bool etw_enabled = true;
  bool stack_spoof_on_read = false;  // red: spoofed call origin
  bool analysis_host = false;        // RE VM / debugger host
  bool canary_tripped = false;       // red canary against analysts
  std::vector<SharedSection> sections;
  int ac_driver_load_order = 50;

  // Minifilter / registry / VEH (wave 3)
  std::size_t minifilter_callbacks = 2;
  bool minifilter_present = true;
  std::size_t registry_notify = 2;
  bool registry_notify_present = true;
  std::size_t veh_handlers = 1;
  bool veh_chain_clean = true;

  // Aim residual (silent aim / challenge)
  std::vector<AimSample> aim_samples;
  bool silent_aim_active = false;

  // Wave 4 scars
  bool physmem_device_open = false;     // MmMapIoSpace-class path
  bool desktop_duplication = false;     // DXGI desktop dup
  bool multibox_net_aim = false;        // remote aim stream
  // Multibox depth: secondary box aim stream residual.
  std::uint32_t multibox_remote_pid = 0;
  int multibox_aim_samples = 0;
  bool multibox_input_desync = false;
  bool overwatch_queued = false;        // blue delayed review
  std::string binary_build_id = "shared";  // polymorphic per-buyer id
  bool mapper_process_present = false;

  // Wave 5 scars
  bool gdi_bitblt_capture = false;
  bool wh_mouse_hook = false;
  bool thread_hide_from_debugger = false;
  bool peb_being_debugged_spoofed = false;
  double time_scale = 1.0;  // speedhack: QPC scale
  bool pool_tag_anomaly = false;
  bool wfp_ndis_filter = false;
  bool ci_options_disabled = false;
  bool feature_control_spoofed = false;
  bool lag_switch_active = false;
  // Lag-switch depth: burst drop samples vs steady packet_loss_faked.
  int lag_switch_drop_bursts = 0;
  float lag_switch_hold_ms = 0.f;
  bool external_display_clone = false;
  bool vpn_proxy_active = false;
  std::string build_watermark;  // per-build watermark string

  // Wave 6 scars
  bool printwindow_capture = false;
  bool block_input_active = false;
  bool sedebug_privilege = false;
  bool raw_sendinput_mixed = false;  // raw_hid + injected mixed improperly
  bool instrumentation_callback = false;
  bool etw_ti_blind = false;  // ETW Threat Intelligence dark
  bool infinity_hook_residual = false;
  bool vtl1_enclave_present = true;  // expected on hardened hosts
  bool packet_loss_faked = false;    // disambig from lag_switch
  bool clipcursor_confined = false;
  double overwatch_score = 0;        // multi-signal score
  bool clipboard_token_leak = false;

  // Server-side truth (for interest / info advantage)
  bool server_sends_full_enemy_origin = true;
  bool entity_stream_encrypted = false;  // structural crypto blob
  bool client_has_stream_key = false;
  bool stream_key_exfiltrated = false;   // red stole session key (depth)
  bool smm_residual = false;             // firmware-class residual (lab only)

  // Remote-read telemetry for bulk/pattern-scan blue sensors.
  // Incremented on successful read_mem when a VmRead handle is required.
  std::uint32_t remote_read_ops = 0;
  std::uint32_t remote_read_bytes = 0;
  // Lab pattern marker planted for educational AOB discovery demos.
  bool lab_pattern_marker_present = false;
  // Byte offset of ACPT marker within game Process::memory (image-relative).
  std::size_t lab_pattern_marker_off = 0x200;
  // Image-relative offset of entity table (count dword); marker payload points here.
  std::uint32_t lab_entity_table_rel = 0;
  // Bumped on plant/mutate so red can detect stale layout and auto-refresh.
  std::uint32_t lab_pattern_generation = 0;
  // How many times red (or lab) reported a re-scan after layout change (blue residual).
  std::uint32_t pattern_rescan_count = 0;
  // Multi-match delayed confidence (ops/structural pedagogy).
  int lab_match_tick = 0;
  double lab_confidence = 0.0;  // accumulates weak signals across ticks
  bool lab_delayed_ban_ready = false;
  // Client fidelity budget after fog+ (1.0 = full origins, 0.0 = none).
  float client_entity_fidelity = 1.0f;
  int client_replicated_enemy_budget = -1;  // -1 unlimited; fog sets finite
  // Spectator / delayed-replay second client (info without local RPM).
  bool spectator_feed_active = false;
  bool spectator_has_delayed_enemy_origin = false;
  // Optional spectator observer target pid/name residual (who is watched).
  std::uint32_t spectator_target_controller = 0;
  int spectator_count = 0;

  // Wave 7 scars — catalog pairs 33–39 (Simulated educational residuals).
  // 33: handle owned by reputable proxy process; cheat consumes without own handle.
  bool handle_proxy_active = false;
  std::uint32_t handle_proxy_owner_pid = 0;
  std::uint32_t handle_proxy_consumer_pid = 0;
  // 34: bomb/planted/defuse timer intel beyond player XY product.
  bool bomb_intel_product = false;
  float bomb_fuse_known = -1.f;
  bool bomb_defuse_known = false;
  float bomb_defuse_progress_known = 0.f;
  std::string bomb_site_known;
  // 36: triggerbot fire-latency residual.
  bool triggerbot_active = false;
  int trigger_on_target_fires = 0;
  float trigger_mean_latency_ms = 0.f;
  // 37: RCS spray-pattern pitch correction residual.
  bool rcs_active = false;
  int rcs_pattern_samples = 0;
  // 38: sound ESP / movement-without-LOS residual.
  bool sound_esp_active = false;
  int sound_inferred_without_los = 0;
  // 39: stream-proof presentation — capture path misses game-present overlays.
  bool capture_sees_overlays = true;

  // Wave 8 scars — P0/P1 archive gaps (schema product + feature residuals).
  // Schema/offset product pipeline (deepens 29 + 14 pedagogy).
  bool schema_cache_active = false;
  std::string schema_cache_version;   // versioned layout token (build/gen)
  std::uint32_t schema_cache_hits = 0;
  bool schema_remote_update = false;  // fetched layout from offset C2 / SaaS
  int schema_fetch_count = 0;
  // 40: no-flash / client FX strip
  bool no_flash_active = false;
  float flash_alpha_forced = 1.f;  // 0 = fully stripped (cheat), 1 = normal
  // 41: grenade/projectile ESP + nade prediction
  bool projectile_esp_active = false;
  int projectile_product_count = 0;
  bool nade_prediction_active = false;
  int nade_predicted_arcs = 0;
  // 42: round-object glow (dropped bomb / defuse kit / hostage) ≠ fuse timer
  bool object_glow_product = false;
  int glow_dropped_bomb = 0;
  int glow_defuse_kit = 0;
  int glow_hostage = 0;
  // Multi-class polish (Osiris-style): additional object classes in one product.
  int glow_grenade_projectile = 0;
  int glow_weapon = 0;
  int glow_ticking_bomb = 0;
  int object_glow_class_count = 0;  // how many distinct classes non-zero
  // 43: plant/defuse HUD timing alerts (feasibility window ≠ fuse known)
  bool plant_hud_alert_active = false;
  bool plant_feasible_before_round_end = false;
  bool defuse_window_alert_active = false;
  float plant_time_remaining_known = -1.f;
  // 44: internal footprint class (CRT/import/thread/heap story beyond inject)
  bool internal_footprint_stealth = false;
  bool crt_runtime_absent = false;
  bool static_imports_cleared = false;
  bool no_cheat_threads = false;
  bool no_heap_alloc_claimed = false;
  // 45: protector suite (anti-debug / anti-suspend / destruct)
  bool anti_debug_active = false;
  bool anti_suspend_active = false;
  bool self_destruct_armed = false;
  bool protector_watchdog = false;
  // Watchdog / anti-suspend micro-detail (gaming-chair class, deeper than flags).
  bool anti_suspend_watchdog_payload = false;
  std::uint32_t anti_suspend_watchdog_pid = 0;
  int watchdog_heartbeat_ticks = 0;
  bool watchdog_payload_hidden = false;

  // Wave 9 scars — P2 secondary archive residuals (presentation / capture / depth).
  // 46: FOV / viewmodel presentation mod
  bool fov_mod_active = false;
  float client_fov_override = 0.f;      // 0 = default; >0 cheat FOV
  float viewmodel_fov_override = 0.f;   // 0 = default
  // 47: sniper no-scope inaccuracy visualization
  bool noscope_inaccuracy_viz = false;
  float noscope_spread_shown = 0.f;
  // 48: sound visualizer subtypes (finer than generic sound ESP 38)
  bool sound_viz_subtypes_active = false;
  int sound_viz_reload = 0;
  int sound_viz_scope = 0;
  int sound_viz_bomb_beep = 0;
  int sound_viz_footstep = 0;
  // Independent panel-class hits blue can score separately (product polish).
  bool sound_panel_reload_hit = false;
  bool sound_panel_scope_hit = false;
  bool sound_panel_bomb_beep_hit = false;
  bool sound_panel_footstep_hit = false;
  // 49: crosshair / sniper-crosshair helpers (≠ soft-aim / trigger alone)
  bool crosshair_helper_active = false;
  bool sniper_crosshair_force = false;
  bool recoil_crosshair = false;
  // 50: desktop-duplication as capture *sensor* (deeper than streamproof 39)
  bool capture_sensor_active = false;     // blue/AC DXGI dup sensor path
  bool present_path_has_overlay = false;  // game present path sees overlay
  bool capture_vs_present_mismatch = false;
  // DXGI composite presentation (separate from the blue capture sensor).
  bool dxgi_composite_active = false;
  bool desktop_duplication_active = false;
  int composite_frames_rendered = 0;
  // Simulated WDA exclusion must be restored after presentation changes.
  bool wda_excluded_from_capture = false;
  int wda_reapply_count = 0;
  // 51: DLL / thread protection detail (deeper than protector_suite 45)
  bool dll_protection_active = false;
  bool thread_protection_active = false;
  bool protector_find_target = false;
  int protected_thread_count = 0;
  // 52: schema as standalone SaaS product residual (≠ one-shot scan 29)
  bool schema_saas_product = false;
  std::string schema_saas_endpoint;
  int schema_cache_file_hits = 0;
  bool schema_version_pin = false;
  // 53: multi-map radar share UX residual (≠ plain web radar 10)
  bool multimap_radar_share = false;
  int radar_maps_shared = 0;
  bool radar_share_ux_active = false;
  std::string radar_share_token;

  // ── Wave 10: Advanced red team evasion scars (red team depth) ──

  // T0: Windowless swapchain hijack — hooks game present path WITHOUT
  // creating a visible overlay window. Harder for blue to detect than
  // external overlay because there's no independent HWND/WNDCLASS.
  bool windowless_swapchain_hijack = false;
  // Game swapchain has been intercepted (injected Present hook) but no
  // OverlayWindow is registered. Blue must detect via module hook scar.
  bool swapchain_hijacked_no_window = false;

  // T0: Scattered entity read — red reads entity table entries in random
  // order with random offsets and timing jitter. Defeats blue pattern
  // detection that relies on sequential offset access.
  bool scattered_read_pattern = false;
  // Number of scattered reads performed.
  int scattered_read_count = 0;
  // Whether timing jitter was applied between reads.
  bool read_timing_jitter = false;

  // T0: Multi-process IPC split — red splits the cheat into 3 processes:
  // (1) handle-holder (legitimate name), (2) reader (no handle, gets data
  // via IPC shared section), (3) overlay/UI (no handle, no read). Harder
  // for blue to correlate because no single process has both handle+UI.
  bool multi_process_split_active = false;
  // PID of the process that holds the game handle.
  std::uint32_t split_holder_pid = 0;
  // PID of the reader process (gets data via IPC section).
  std::uint32_t split_reader_pid = 0;
  // PID of the UI/overlay process.
  std::uint32_t split_ui_pid = 0;

  // T1: Heaven's Gate syscall — 32-bit process transitions to 64-bit
  // mode via the wow64 CPU switch and executes a syscall directly,
  // bypassing the native 64-bit ntdll entirely. Leaves no via_syscall_path
  // handle because the open uses a different code path.
  bool heavens_gate_transition = false;
  // The 32-bit stub PID that performed the Heaven's Gate transition.
  std::uint32_t heavens_gate_stub_pid = 0;
  // T1: Enhanced stack spoof — full call stack forgery with multiple
  // spoofed return addresses. Depth tracks how many frames were forged.
  bool enhanced_stack_spoof = false;
  int spoofed_call_depth = 0;

  // T1: Dynamic SSN resolution — red resolves syscall numbers from disk
  // (ntdll.dll) at runtime instead of using hardcoded tables. Defeats
  // blue SSN fingerprinting because the resolved numbers match the exact
  // build of Windows on the target machine.
  bool dynamic_ssn_resolved = false;
  // The Windows build number from which SSNs were resolved.
  int resolved_ssn_windows_build = 0;

  // T1: NTDLL hook detection and evasion — red checks if ntdll functions
  // are hooked before calling them. If hooked, red either syscalls directly
  // or copies a clean ntdll from disk. Defeats usermode API hooking.
  bool ntdll_hooks_detected = false;
  // Number of hooks detected and evaded.
  int hooks_evaded_count = 0;
  // Whether red used a clean mapped copy of ntdll instead.
  bool used_clean_ntdll_copy = false;

  // T2: \\.\PhysicalMemory direct mapping — opens and maps physical
  // memory device directly, bypassing IOCTL interface. No driver needed
  // when DSE is off or via vulnerable driver.
  bool physmem_direct_mapped = false;
  // Physical offset of the game's memory pages for direct read.
  std::uint64_t physmem_game_pfn = 0;

  // T2: DKOM token steal — red steals a SYSTEM-equivalent token from
  // another process via DKOM and uses it to open the game with full
  // privileges, bypassing handle access checks.
  bool dkom_token_stolen = false;
  // The PID from which the token was stolen.
  std::uint32_t dkom_token_source_pid = 0;
  // Whether the stolen token grants bypass of handle ACL checks.
  bool token_bypasses_handle_acls = false;

  // T2: ACPI PM timer memory read — red triggers an SMI via the ACPI
  // Power Management timer (PM_TMR) to read physical memory through an
  // SMM handler. Bypasses all kernel callbacks because SMM runs below
  // the kernel. No driver needed when ACPI methods can trigger SMIs.
  bool acpi_pm_read_active = false;
  // Number of SMIs triggered for memory read.
  int acpi_smi_trigger_count = 0;

  // T2: Hypercall memory read — red uses hypercalls (via existing
  // platform hypervisor like Hyper-V) to read physical memory. Even
  // without a personal HV, red can use the platform hypervisor's
  // hypercall interface if VBS is disabled or vulnerable hypercalls
  // are exposed. No kernel driver needed.
  bool hypercall_read_active = false;
  // Which hypervisor was used: "hyperv", "vmware", "native_intel".
  std::string hypercall_vendor;

