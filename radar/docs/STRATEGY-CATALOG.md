# Strategy catalog

**Live truth:** `./build/strategy_lab list` and `code/strategies/framework/registry.cpp`.

There are on the order of **~180+ pair folders** under `code/strategies/{t0,t1,t2,t3,t4,crosscutting}/`. IDs are not a clean 1–N sequence (waves reused some numbers with different names). Treat this file as a **human index of the classic 01–99 narrative pairs**; the CLI may list additional wave / crosscutting entries that are not every row below.

```bash
cd code
cmake --build build -j
./build/strategy_lab list
./build/strategy_lab list --family Delivery
./build/strategy_lab stats
./build/strategy_lab run 01_external_rpm
./build/strategy_lab all --quiet
```

Path-oriented index: `code/strategies/CATALOG.md`.

## Classic catalog (01–99)

| ID | Title | Tiers | Family | Red | Blue |
|----|-------|-------|--------|-----|------|
| `01_external_rpm` | External usermode RPM | T0 | Delivery | OpenProcess + ReadProcessMemory from another process | Enumerate handles with VM_READ into the game |
| `02_indirect_syscall` | Indirect syscalls | T1 | Delivery | Syscall open/read bypasses usermode hooks | Do not trust ntdll hooks; use handle graph |
| `03_kernel_ioctl` | Kernel IOCTL read | T2 | Delivery | Driver R/W without usermode game handle | Image load + device/IOCTL correlation |
| `04_byovd` | Bring Your Own Vulnerable Driver | T2 | Delivery | Signed vuln driver for kernel R/W | Cloud-updated vulnerable driver blocklist |
| `05_hypervisor` | Personal hypervisor read | T3 | Delivery | HV introspection + bridge UI | VBS/HVCI policy, HV probes, bridge driver intel |
| `06_dma_hardware` | DMA / hardware off-box | T4 | Delivery | PCIe DMA second machine via full residual loop, no local process | Multi-sensor IOMMU + fog-of-war + info-advantage |
| `07_internal_inject` | Internal inject | T0-T1 | Delivery | DLL into game process | Module list / image load notify |
| `08_manual_map_hide` | Manual map / PEB unlink | T1 | Evasion | Hide module from PEB lists | Scan private executable regions + threads, not only PEB |
| `09_overlay_esp` | Overlay ESP | T0-T1 | Feature | Draw boxes via overlay/hijack + data handle | Composition + handle correlation (not OR alone) |
| `10_web_phone_radar` | Web/phone radar feed | T0-T2 | Feature | Stream entities to phone UI via PC reader + SaaS | Detect PC reader path ∧ SaaS (overlay clean narrative) |
| `11_aim_humanization` | Aim humanization | all | Feature | Soft aim distributions look human | Don't rely on snap-only ML; add info-advantage |
| `12_staged_loader` | Staged loader | T1-T3 | Evasion | Stub downloads/maps real logic | Runtime memory + launch lineage + C2 intel |
| `13_obfuscation_crypto` | Obfuscation & crypto | all | Evasion | Encrypt strings/offsets; VMProtect; polymorphic builds | Never depend on static alone; hunt scars |
| `14_offset_c2` | Offset / schema C2 | all | Evasion | Post-patch encrypted offset delivery | Detect readers; C2 is supportive intel |
| `15_hwid_spoof` | HWID spoofing | all | Evasion | Change hardware identity after ban | Cluster payment/IP/social; soft HWID |
| `16_callback_strip` | Kernel callback strip | T2 | Evasion | Tamper notify chains to blind AC | Continuous callback chain integrity |
| `17_handle_minimize` | Handle access minimization | T0-T1 | Evasion | Narrow rights / brief reopen | Continuous handle sampling still sees edges |
| `18_read_throttle` | Sparse/throttled reads | T0-T2 | Evasion | Lower read rate and field set | Attribute reads to foreign principal, not volume alone |
| `19_input_synthesis` | Synthetic / hardware input | all | Feature | Mouse via inject or external MCU multi-sample | Input provenance multi-reason (bad source + multi/mixed) |
| `20_fallback_chain` | Tier fallback chain | all | Delivery | Degrade HV→kernel→RPM automatically | Always run full detector stack |
| `21_module_integrity` | Game module integrity | T0-T1 internal | Detection | Patch/hooks game code | Continuous .text hashing (necessary, not sufficient) |
| `22_input_provenance` | Input provenance (blue) | all | Detection | Multi-sample injected+raw_hid mix; raw_sendinput_mixed residual | Correlate inject with raw/mixed/multi_sample (not single echo) |
| `23_account_graph` | Account / social graph | all | Detection | Multi-account payment/IP links + VPN + build/C2 | SellerFusion multi_reason linked_pairs&&(vpn||cluster) |
| `24_interest_mgmt` | Server interest management | all | Structural | Read whatever client has | Don't send free enemy XY when not observable |
| `25_info_advantage` | Info-advantage behavior | all | Structural | Play with unfair knowledge, human aim | Score pre-aim/track without vision or sound |
| `26_delayed_ban` | Delayed / correlated bans | all | Structural | Stay under instant thresholds with weak multi-scars | Multi-signal delayed confidence bans from world scars |
| `27_boot_trust` | Boot & platform trust | T3 | Structural | Disable SB/VBS; early loaders | Ranked attestation + boot driver allowlist |
| `28_network_c2_intel` | Network / C2 intel | all | Detection | Loader + VmRead handle + auth/offset-C2/radar SaaS | multi_reason intel_hits&&handle_hit pair_with_client_scar |
| `29_process_cooccurrence` | Process co-occurrence radar | T0 | Detection | Co-run lab-radar/esp-named process with VM_READ on the live game | list_processes name heuristic AND/OR handle graph foreign VM_READ |
| `30_fp_allowlist_evasion` | FP allowlist evasion | T0 | Evasion | Masquerade as nvidia-overlay + looks_reputable, still OpenProcess(VM_READ) | Name allowlist alone misses; still detect foreign VM_READ (reputation != safe) |
| `31_parent_lineage` | Parent lineage spoof | T1 | Evasion | Launch under explorer/Discord so process tree looks benign | Join handle graph with parent lineage; flag shell→cheat edges |
| `32_inmatch_only` | In-match-only reader | T1 | Evasion | Open game handle only while match_active; stay dark in lobby | Sample during match: foreign VM_READ or reader_active flag |
| `33_driver_allowlist` | Ranked driver allowlist | T2 | Delivery | Private memrw sha outside ranked allowlist | Strict allowlist: unknown driver hash is a scar |
| `34_scm_service` | SCM kernel-driver service | T2 | Delivery | Memrw registered as kernel-driver SCM service | Inventory kernel services; flag non-AC owners |
| `35_callback_shadow` | Callback shadow façade | T2 | Evasion | Degrade true notifies; clean single-shot sample | Sample + compare process_notify_true / shadow flag |
| `36_nested_hv` | Nested personal hypervisor | T3 | Delivery | Platform HV + disable VBS; personal ACLABHV | Detect personal under Microsoft-only platform; policy fail |
| `37_attestation` | Platform attestation gate | T3 | Structural | Break quote signature / PCR match | Ranked deny if attestation invalid or PCR not known-good |
| `38_timing_spoof` | HV timing spoof | T3 | Evasion | Personal HV; fake CPUID latency near baseline | Multi-invariant: vendor / spoof flag / latency×vendor |
| `39_capture_cv_hid` | Capture card CV + HID | T4 | Delivery | Vision from capture card; multi-sample serial MCU — no memory scar | Multi-reason capture+input; fog residual |
| `40_entity_stream_crypto` | Entity stream crypto | all | Structural | Read/decrypt client entity stream when key is present | Encrypt without client key + interest management |
| `41_ac_self_integrity` | AC self-integrity | all | Detection | Multi-step: AC text_hash=tampered + iat/headers residual | Dirty is_ac modules + hook residual; multi_reason; fail closed |
| `42_handle_hide_on_enum` | Handle hide on enum | T0 | Evasion | OpenProcess(VM_READ) then hide handle during AC enumeration | Single-sample may miss; count_hidden / continuous include_hidden detects |
| `43_section_map` | Section / mapped entity stream | T0 | Evasion | SharedSection carries entity bytes; optional brief open then close | Scan sections: carries_entity_bytes + non-AC creator |
| `44_stack_spoof` | Stack spoof on read | T1 | Evasion | Syscall open/read with stack_spoof_on_read to lie to stack walkers | Foreign VM_READ detects alone; +stack_spoof_on_read = spoofed origin |
| `45_etw_blind` | ETW pipeline blind | T1 | Evasion | Set etw_enabled=false then open/read so ETW consumers go dark | Detect !etw_enabled as pipeline broken; handle graph still works |
| `46_early_load_race` | Early driver load race | T2 | Delivery | Memrw loads before AC (load_order < ac_driver_load_order) | Audit non-AC memrw with load_order before AC |
| `47_object_callback_strip` | Object callback strip | T2 | Evasion | Strip Ob* handle callbacks (not process notify) | Baseline object_callbacks / present; detect degradation |
| `48_ept_hide_ac` | EPT-hide AC pages | T3 | Evasion | Personal HV; EPT remaps AC so guest self-hash stays clean | Detect ept_hide_ac_pages / personal_hv+hide; ranked deny |
| `49_efi_boot_entry` | Unexpected EFI boot entry | T3 | Structural | Plant cheat-loader.efi; often Secure Boot off | Detect unexpected_efi_entry; ranked policy deny |
| `50_dual_boot_posture` | Dual-boot posture | T4 | Delivery | Dual-boot cheat OS + PCR + DMA residual | Multi-sensor dual_boot/PCR/DMA; fog + ranked deny |
| `51_report_velocity` | Report velocity | all | Detection | ≥3 accounts same payment high reports + clean control | multi_reason high_report_accounts&&payment_cluster |
| `52_anti_re_canary` | Anti-RE canary | all | Evasion | Trip canary on analysis_host; refuse/decoy payload | Detect analysis_host || canary_tripped |
| `53_thread_hijack` | Thread hijack / APC scar | T0 | Evasion | Spawn helper; mark game thread_hijacked + has_foreign_thread (optional handle) | Any is_game with thread_hijacked || has_foreign_thread (non-AC context) |
| `54_module_stomp` | Module stomping | T1 | Evasion | Stomp client.dll .text (text_hash=stomped) inside game process | Detect known modules (client.dll/game.exe) with text_hash != clean |
| `55_process_hollow` | Process hollowing | T1 | Evasion | Hollow svchost.exe (looks_reputable), map region, open VM_READ | Detect hollowed or (reputable + manual_mapped + foreign VM_READ) |
| `56_minifilter_strip` | Minifilter strip | T2 | Evasion | Strip FsRtl/FltMgr minifilter callbacks (path sensors) | Baseline minifilter_callbacks / present; detect degradation |
| `57_registry_notify_strip` | Registry notify strip | T2 | Evasion | Strip CmRegisterCallback-style registry notifies | Baseline registry_notify / present; detect degradation |
| `58_elam_bypass` | ELAM / Secure Launch bypass | T3 | Evasion | Clear ELAM+Secure Launch; plant early non-AC boot driver | Ranked deny if !elam||!secure_launch; early boot audit |
| `59_cr3_stealth_target` | CR3 stealth target | T3 | Evasion | Personal HV; target by CR3; hv_read without name-based open | Detect personal_hv+no VM_READ / bridge+cr3; policy fail |
| `60_aim_challenge` | Aim challenge residual | T4 | Delivery | Multi-sample capture/serial aim fails server challenge | Multi-reason challenge+input+capture; fog |
| `61_iommu_policy` | IOMMU ranked policy | T4 | Delivery | DMA+IOMMU-off + optional dma_read; policy unset by red | ranked_requires_iommu; multi-reason deny + fog |
| `62_silent_aim_desync` | Silent aim desync | all | Feature | silent_aim_active with camera/server aim angle desync | Detect silent_aim flag or abs(camera-server) > threshold |
| `63_veh_exception_cf` | VEH exception CF | all | Evasion | Elevate VEH handlers or dirty veh_chain_clean | Detect !veh_chain_clean || veh_handlers > baseline |
| `64_iat_eat_hook` | IAT / EAT hook scar | T0 | Evasion | Set client.dll iat_hooked and/or eat_hooked on the game process | Any is_game module with iat_hooked || eat_hooked |
| `65_dxgi_present_hook` | DXGI Present hook scar | T0 | Feature | Set present_hooked on game.exe or client.dll; optional overlay | Any is_game module with present_hooked |
| `66_mapper_artifact` | Mapper process artifact | T1 | Delivery | Spawn kdmapper/mapper.exe; set mapper_process_present; unsigned path | Detect mapper_process_present or process name contains mapper/kdmapper |
| `67_dse_testsign` | DSE / test-signing posture | T1 | Evasion | dse_enforced=false, test_signing=true | Ranked posture deny if !dse_enforced || test_signing |
| `68_dkom_hide` | DKOM process hide | T2 | Evasion | Hide cheat from weak process enum; keep handle or reader_active | Strong list_processes / handle graph when weak list is empty |
| `69_physmem_map` | Physmem map path | T2 | Delivery | physmem_device_open + memrw driver; no usermode game handle | Detect physmem open or memrw without foreign VM_READ |
| `70_secure_kernel_view` | Secure Kernel dual-view | T3 | Evasion | Guest AC self-hash clean while Secure Kernel sees tamper | Detect secure_kernel_view_dirty even if guest clean; ranked deny |
| `71_hvci_race` | HVCI race / continuous policy | T3 | Evasion | Race or disable HVCI (hvci_enabled=false); optional personal HV | Continuous policy: !hvci_enabled → ranked deny / detect |
| `72_desktop_duplication` | Desktop duplication residual | T4 | Delivery | DXGI desktop dup + clean process; no OpenProcess | Multi-reason desktop_dup+clean; fog mitigate |
| `73_network_multibox_aim` | Network multibox aim | T4 | Delivery | Remote aim stream + multi-sample serial/kmbox + net residual | Multi-reason multibox or net+HID correlation; fog |
| `74_overwatch_queue` | Overwatch delayed queue | all | Detection | Mild handle + no_rage + second weak (vpn/report) | weak_handle + multi-invariant; multi_reason handle&&overwatch |
| `75_polymorphic_build` | Polymorphic per-buyer build | all | Evasion | Unique binary_build_id + buyer process + handle + wm/c2 | Hash miss; multi-reason handle+(poly_cluster|wm) — not hash-only |
| `76_gdi_bitblt` | GDI BitBlt screen capture | T0 | Feature | gdi_bitblt_capture=true; no OpenProcess to game | Detect gdi_bitblt_capture; fog mitigate optional |
| `77_wh_mouse_hook` | WH_MOUSE / mouse hook scar | T0 | Feature | wh_mouse_hook=true; optional inject inputs | Detect wh_mouse_hook |
| `78_thread_hide_dbg` | ThreadHideFromDebugger / PEB spoof | T1 | Evasion | thread_hide_from_debugger=true, peb_being_debugged_spoofed=true; may open handle | Detect either thread_hide_from_debugger or peb_being_debugged_spoofed |
| `79_speedhack_timescale` | Speedhack time scale | T1 | Feature | time_scale = 1.5 or 2.0 (QPC speedhack) | Detect time_scale != 1.0 (small epsilon) |
| `80_pool_tag_hide` | Pool-tag hide residual | T2 | Evasion | Memrw driver + pool-tag hide while IOCTL-reading entities | KernelAc + pool inventory vs kernel surface |
| `81_wfp_ndis_filter` | WFP/NDIS packet filter | T2 | Evasion | Kernel reader + WFP/NDIS filter + lag + SCM filter service | KernelAc + WFP/SCM/lag multi-signal detect |
| `82_ci_options` | Code integrity options disabled | T3 | Structural | Disable CI options (ci_options_disabled=true) | Ranked deny if ci_options_disabled |
| `83_feature_control_msr` | Feature-control MSR spoof | T3 | Evasion | Spoof IA32_FEATURE_CONTROL; optional personal HV | Detect feature_control_spoofed; ranked policy deny |
| `84_lag_switch` | Lag switch residual | T4 | Delivery | lag_switch_active + net residual; distinct from packet_loss | Detect lag; disambig vs packet_loss; fog mitigate |
| `85_external_clone_display` | External clone display residual | T4 | Delivery | external_display_clone + clean process; no OpenProcess | Multi-reason clone+clean; fog mitigate |
| `86_vpn_proxy_graph` | VPN / proxy account graph | all | Detection | vpn_proxy_active; ≥3 accounts share vpn_res_1; optional payment | multi_reason vpn_flag&&ip_cluster |
| `87_build_watermark` | Per-buyer build watermark | all | Evasion | wm_buyer_deadbeef + unique id; handle or account link | Multi-reason watermark && (handle || poly_link) — not wm alone |
| `88_printwindow` | PrintWindow screen capture | T0 | Feature | printwindow_capture=true; no OpenProcess to game | Detect printwindow_capture; fog mitigate optional |
| `89_block_input` | BlockInput session lock | T0 | Feature | block_input_active=true | Detect block_input_active |
| `90_sedebug_priv` | SeDebugPrivilege enable | T1 | Evasion | sedebug_privilege=true; may open handle | Detect sedebug_privilege |
| `91_raw_vs_sendinput` | Raw HID vs SendInput mix | T1 | Feature | raw_sendinput_mixed=true; push raw_hid + injected | Detect raw_sendinput_mixed or mixed sources in inputs |
| `92_instr_callback` | Instrumentation callback residual | T2 | Evasion | Kernel reader + InstrCallback residual + stack spoof | KernelAc + InstrCallback integrity correlation |
| `93_etw_ti_blind` | ETW Threat Intelligence blind | T2 | Evasion | Kernel IOCTL radar + darken ETW/ETW-TI while no game handle | KernelAc + TI integrity correlation |
| `94_infinity_hook` | InfinityHook residual | T3 | Evasion | Leave infinity_hook_residual; optional personal HV | Detect infinity_hook_residual; optional ranked policy deny |
| `95_vtl1_enclave_miss` | VTL1 enclave missing | T3 | Structural | Clear expected VTL1 enclave (vtl1_enclave_present=false) | Ranked deny if !vtl1_enclave_present |
| `96_packet_loss_disambig` | Packet loss vs lag-switch disambig | T4 | Delivery | packet_loss_faked without lag_switch; net residual optional | Disambig loss_only; multi-reason + fog |
| `97_clipcursor` | ClipCursor confine residual | T4 | Delivery | clipcursor_confined + HID; no game handle | Multi-reason confine+HID; fog mitigate |
| `98_overwatch_multisignal` | Overwatch multi-signal score | all | Detection | Handle + vpn + unique build; overwatch_score starts 0 | Score≥2 from multi signals; queue; multi_reason handle+vpn+poly |
| `99_clipboard_token` | Clipboard token leak | all | Evasion | leak + loader + CDN/auth net; optional handle/mapper | Multi-reason leak && (net||loader||handle) — not bool-echo |

## Path index by area

See `code/strategies/CATALOG.md` for the path-oriented index (all strategies under `code/strategies/`).

## Adding a strategy

1. Create `red_example` / `blue_example` / `pair.cpp` under the correct tier or crosscutting folder.  
2. Register `entry_NN_*` in `code/strategies/framework/registry.cpp` (CMake GLOBs `strategies/**/*.cpp`).  
3. Multi-step red + multi-reason blue; LESSON.md accuracy.  
4. Extend the relevant `*_full_tests` if the pair is a new lesson class.  
5. Update this file via `strategy_lab list` regeneration when shipping docs.

## 29_pattern_offset_scan (T0 Delivery)

External bulk pattern scan for lab `ACPT` marker → entity RPM. Blue: handle + bulk reads + co-occurrence; fog mitigate.
