# Strategy catalog (path index)

**Live truth:** `./build/strategy_lab list` and `framework/registry.cpp`.

Rough size: ~180+ pair folders under `t0/` … `t4/` and `crosscutting/`. IDs are not a pure sequence (later waves added higher numbers and some reused prefixes). Tables below are a path index — regenerate mentally from the tree when they drift.

Full one-liners: [`../../docs/STRATEGY-CATALOG.md`](../../docs/STRATEGY-CATALOG.md).

```bash
./build/strategy_lab list
./build/strategy_lab stats
./build/strategy_lab run <id>
./build/strategy_lab all [--quiet] [--family Family] [--tier substr]
```

Locations are tier-first: `strategies/<tier>/<folder>/`.

## T0 — `strategies/t0/`

| ID | Folder |
|----|--------|
| 01_external_rpm | external_rpm |
| 29_pattern_offset_scan | pattern_offset_scan |
| 33_handle_hijack_proxy | handle_hijack_proxy |
| 07_internal_inject | internal_inject |
| 17_handle_minimize | handle_minimize |
| 18_read_throttle | read_throttle |
| 21_module_integrity | module_integrity |
| 29_process_cooccurrence | process_cooccurrence |
| 30_fp_allowlist_evasion | fp_allowlist_evasion |
| 42_handle_hide_on_enum | handle_hide_on_enum |
| 43_section_map | section_map |
| 53_thread_hijack | thread_hijack |
| 64_iat_eat_hook | iat_eat_hook |
| 65_dxgi_present_hook | dxgi_present_hook |
| 76_gdi_bitblt | gdi_bitblt |
| 77_wh_mouse_hook | wh_mouse_hook |
| 88_printwindow | printwindow |
| 89_block_input | block_input |

## T1 — `strategies/t1/`

| ID | Folder |
|----|--------|
| 02_indirect_syscall | indirect_syscall |
| 08_manual_map_hide | manual_map_hide |
| 31_parent_lineage | parent_lineage |
| 32_inmatch_only | inmatch_only |
| 44_stack_spoof | stack_spoof |
| 45_etw_blind | etw_blind |
| 54_module_stomp | module_stomp |
| 55_process_hollow | process_hollow |

## T2 — `strategies/t2/`

| ID | Folder |
|----|--------|
| 03_kernel_ioctl | kernel_ioctl |
| 04_byovd | byovd |
| 16_callback_strip | callback_strip |
| 33_driver_allowlist | driver_allowlist |
| 34_scm_service | scm_service |
| 35_callback_shadow | callback_shadow |
| 46_early_load_race | early_load_race |
| 47_object_callback_strip | object_callback_strip |

## T3 — `strategies/t3/`

| ID | Folder |
|----|--------|
| 05_hypervisor | hypervisor |
| 27_boot_trust | boot_trust |
| 36_nested_hv | nested_hv |
| 37_attestation | attestation |
| 38_timing_spoof | timing_spoof |
| 48_ept_hide_ac | ept_hide_ac |
| 49_efi_boot_entry | efi_boot_entry |
| 58_elam_bypass | elam_bypass |
| 59_cr3_stealth_target | cr3_stealth_target |
| 70_secure_kernel_view | secure_kernel_view |
| 71_hvci_race | hvci_race |
| 82_ci_options | ci_options |
| 83_feature_control_msr | feature_control_msr |

## T4 — `strategies/t4/`

| ID | Folder |
|----|--------|
| 06_dma_hardware | dma_hardware |
| 39_capture_cv_hid | capture_cv_hid |
| 50_dual_boot_posture | dual_boot_posture |
| 60_aim_challenge | aim_challenge |
| 61_iommu_policy | iommu_policy |
| 72_desktop_duplication | desktop_duplication |
| 73_network_multibox_aim | network_multibox_aim |
| 84_lag_switch | lag_switch |
| 85_external_clone_display | external_clone_display |
| 96_packet_loss_disambig | packet_loss_disambig |
| 97_clipcursor | clipcursor |

## Crosscutting

| ID | Path |
|----|------|
| 12_staged_loader | crosscutting/staged_loader |
| 13_obfuscation_crypto | crosscutting/obfuscation_crypto |
| 14_offset_c2 | crosscutting/offset_c2 |
| 15_hwid_spoof | crosscutting/hwid_spoof |
| 52_anti_re_canary | crosscutting/anti_re_canary |
| 63_veh_exception_cf | crosscutting/veh_exception_cf |
| 75_polymorphic_build | crosscutting/polymorphic_build |
| 87_build_watermark | crosscutting/build_watermark |
| 99_clipboard_token | crosscutting/clipboard_token |
| 09_overlay_esp | crosscutting/overlay_esp |
| 10_web_phone_radar | crosscutting/web_phone_radar |
| 11_aim_humanization | crosscutting/aim_humanization |
| 19_input_synthesis | crosscutting/input_synthesis |
| 36_triggerbot_timing | crosscutting/triggerbot_timing |
| 37_rcs_pattern | crosscutting/rcs_pattern |
| 38_sound_esp | crosscutting/sound_esp |
| 39_streamproof_overlay | crosscutting/streamproof_overlay |
| 40_no_flash | crosscutting/no_flash |
| 41_projectile_nade_esp | crosscutting/projectile_nade_esp |
| 42_object_glow_product | crosscutting/object_glow_product |
| 43_plant_defuse_hud | crosscutting/plant_defuse_hud |
| 44_internal_footprint | crosscutting/internal_footprint |
| 45_protector_suite | crosscutting/protector_suite |
| 62_silent_aim_desync | crosscutting/silent_aim_desync |
| 20_fallback_chain | crosscutting/fallback_chain |
| 24_interest_mgmt | crosscutting/interest_mgmt |
| 25_info_advantage | crosscutting/info_advantage |
| 26_delayed_ban | crosscutting/delayed_ban |
| 31_composition_radar_loop | crosscutting/composition_radar_loop |
| 32_cs_round_radar | crosscutting/cs_round_radar |
| 34_bomb_round_intel | crosscutting/bomb_round_intel |
| 35_spectator_feed | crosscutting/spectator_feed |
| 40_entity_stream_crypto | crosscutting/entity_stream_crypto |
| 22_input_provenance | crosscutting/input_provenance |
| 23_account_graph | crosscutting/account_graph |
| 28_network_c2_intel | crosscutting/network_c2_intel |
| 41_ac_self_integrity | crosscutting/ac_self_integrity |
| 51_report_velocity | crosscutting/report_velocity |
| 74_overwatch_queue | crosscutting/overwatch_queue |
| 86_vpn_proxy_graph | crosscutting/vpn_proxy_graph |
| 98_overwatch_multisignal | crosscutting/overwatch_multisignal |


## P2 residuals (46–53)

| ID | Path |
|----|------|
| 46_fov_viewmodel_mod | crosscutting/fov_viewmodel_mod |
| 47_noscope_inaccuracy_viz | crosscutting/noscope_inaccuracy_viz |
| 48_sound_viz_subtypes | crosscutting/sound_viz_subtypes |
| 49_crosshair_helper | crosscutting/crosshair_helper |
| 50_desktop_dup_capture | crosscutting/desktop_dup_capture |
| 51_dll_thread_protect | crosscutting/dll_thread_protect |
| 52_schema_saas_product | crosscutting/schema_saas_product |
| 53_multimap_radar_share | crosscutting/multimap_radar_share |


## T4 polish promotions (54–56)

| ID | Path |
|----|------|
| 54_lag_switch | crosscutting/lag_switch |
| 55_network_multibox_aim | crosscutting/network_multibox_aim |
| 56_iommu_policy | crosscutting/iommu_policy |
