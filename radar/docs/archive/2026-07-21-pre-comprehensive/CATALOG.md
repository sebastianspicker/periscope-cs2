# Strategy catalog (paths after restructure)

```bash
./build/strategy_lab list
./build/strategy_lab run <id>
./build/strategy_lab all
```

IDs are stable (`01_external_rpm`, …). Locations are tier-first.

## T0 — `tiers/t0_usermode_rpm/strategies/`

| ID | Folder |
|----|--------|
| 01_external_rpm | external_rpm |
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

## T1 — `tiers/t1_syscall_soft/strategies/`

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

## T2 — `tiers/t2_kernel_byovd/strategies/`

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

## T3 — `tiers/t3_hypervisor/strategies/`

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

## T4 — `tiers/t4_dma_residual/strategies/`

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
| 12_staged_loader | crosscutting/evasion/staged_loader |
| 13_obfuscation_crypto | crosscutting/evasion/obfuscation_crypto |
| 14_offset_c2 | crosscutting/evasion/offset_c2 |
| 15_hwid_spoof | crosscutting/evasion/hwid_spoof |
| 52_anti_re_canary | crosscutting/evasion/anti_re_canary |
| 63_veh_exception_cf | crosscutting/evasion/veh_exception_cf |
| 75_polymorphic_build | crosscutting/evasion/polymorphic_build |
| 87_build_watermark | crosscutting/evasion/build_watermark |
| 99_clipboard_token | crosscutting/evasion/clipboard_token |
| 09_overlay_esp | crosscutting/features/overlay_esp |
| 10_web_phone_radar | crosscutting/features/web_phone_radar |
| 11_aim_humanization | crosscutting/features/aim_humanization |
| 19_input_synthesis | crosscutting/features/input_synthesis |
| 62_silent_aim_desync | crosscutting/features/silent_aim_desync |
| 20_fallback_chain | crosscutting/structural/fallback_chain |
| 24_interest_mgmt | crosscutting/structural/interest_mgmt |
| 25_info_advantage | crosscutting/structural/info_advantage |
| 26_delayed_ban | crosscutting/structural/delayed_ban |
| 40_entity_stream_crypto | crosscutting/structural/entity_stream_crypto |
| 22_input_provenance | crosscutting/ops/input_provenance |
| 23_account_graph | crosscutting/ops/account_graph |
| 28_network_c2_intel | crosscutting/ops/network_c2_intel |
| 41_ac_self_integrity | crosscutting/ops/ac_self_integrity |
| 51_report_velocity | crosscutting/ops/report_velocity |
| 74_overwatch_queue | crosscutting/ops/overwatch_queue |
| 86_vpn_proxy_graph | crosscutting/ops/vpn_proxy_graph |
| 98_overwatch_multisignal | crosscutting/ops/overwatch_multisignal |
