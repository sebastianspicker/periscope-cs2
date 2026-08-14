// registry.cpp — strategy catalog table only.
// Forward-declares all entry_NN factories and assembles the catalog vector.
// Run/dual-mode logic lives in registry_run.cpp and registry_dual.cpp.

#include "strategies/framework.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace strategies {

// family_name / iequals live in lib/strategies/framework_meta.cpp (ac_strategy_core).

// ── Entry forward declarations ────────────────────────────────────
StrategyEntry entry_01_external_rpm();
StrategyEntry entry_02_indirect_syscall();
StrategyEntry entry_03_kernel_ioctl();
StrategyEntry entry_04_byovd();
StrategyEntry entry_05_hypervisor();
StrategyEntry entry_06_dma_hardware();
StrategyEntry entry_07_internal_inject();
StrategyEntry entry_08_manual_map_hide();
StrategyEntry entry_09_overlay_esp();
StrategyEntry entry_10_web_phone_radar();
StrategyEntry entry_11_aim_humanization();
StrategyEntry entry_111_staged_loader();
StrategyEntry entry_106_obfuscation_crypto();
StrategyEntry entry_14_offset_c2();
StrategyEntry entry_110_hwid_spoof();
StrategyEntry entry_16_callback_strip();
StrategyEntry entry_17_handle_minimize();
StrategyEntry entry_18_read_throttle();
StrategyEntry entry_19_input_synthesis();
StrategyEntry entry_20_fallback_chain();
StrategyEntry entry_21_module_integrity();
StrategyEntry entry_22_input_provenance();
StrategyEntry entry_23_account_graph();
StrategyEntry entry_24_interest_mgmt();
StrategyEntry entry_25_info_advantage();
StrategyEntry entry_26_delayed_ban();
StrategyEntry entry_27_boot_trust();
StrategyEntry entry_28_network_c2_intel();
StrategyEntry entry_29_pattern_offset_scan();
StrategyEntry entry_30_fp_allowlist_evasion();
StrategyEntry entry_31_composition_radar_loop();
StrategyEntry entry_32_cs_round_radar();
StrategyEntry entry_33_handle_hijack_proxy();
StrategyEntry entry_34_bomb_round_intel();
StrategyEntry entry_35_spectator_feed();
StrategyEntry entry_36_triggerbot_timing();
StrategyEntry entry_37_rcs_pattern();
StrategyEntry entry_38_sound_esp();
StrategyEntry entry_39_streamproof_overlay();
StrategyEntry entry_40_no_flash();
StrategyEntry entry_41_projectile_nade_esp();
StrategyEntry entry_42_object_glow_product();
StrategyEntry entry_43_plant_defuse_hud();
StrategyEntry entry_44_internal_footprint();
StrategyEntry entry_45_protector_suite();
StrategyEntry entry_46_fov_viewmodel_mod();
StrategyEntry entry_47_noscope_inaccuracy_viz();
StrategyEntry entry_48_sound_viz_subtypes();
StrategyEntry entry_49_crosshair_helper();
StrategyEntry entry_50_desktop_dup_capture();
StrategyEntry entry_51_dll_thread_protect();
StrategyEntry entry_52_schema_saas_product();
StrategyEntry entry_53_multimap_radar_share();
StrategyEntry entry_54_lag_switch();
StrategyEntry entry_55_network_multibox_aim();
StrategyEntry entry_56_iommu_policy();
StrategyEntry entry_57_windowless_swapchain();
StrategyEntry entry_58_heavens_gate_syscall();
StrategyEntry entry_59_enhanced_stack_spoof();
StrategyEntry entry_60_physmem_direct_read();
StrategyEntry entry_61_dkom_token_steal();
StrategyEntry entry_62_smm_read_channel();
StrategyEntry entry_63_fpga_smart_dma();
StrategyEntry entry_64_scattered_entity_read();
StrategyEntry entry_65_multi_process_ipc_split();
StrategyEntry entry_66_dynamic_ssn_resolve();
StrategyEntry entry_67_ntdll_hook_evade();
StrategyEntry entry_68_acpi_pm_mem_read();
StrategyEntry entry_69_hypercall_mem_read();
StrategyEntry entry_70_ept_violation_evade();
StrategyEntry entry_71_vmexit_keylog_capture();
StrategyEntry entry_72_pcie_peer_dma();
StrategyEntry entry_100_skin_changer();
StrategyEntry entry_101_gaming_chair();
StrategyEntry entry_102_vac_handle_enum();
StrategyEntry entry_103_thread_monitor_evade();
StrategyEntry entry_104_vmt_integrity();
StrategyEntry entry_105_diagnostic_telemetry();
StrategyEntry entry_112_vmt_proxy_evade();
StrategyEntry entry_113_self_obfuscate_process();
StrategyEntry entry_114_proxy_hijack_reader();
StrategyEntry entry_115_convars_temp_restore();
StrategyEntry entry_116_etw_dual_provider();
StrategyEntry entry_117_thread_4kb_signature();
StrategyEntry entry_118_bsecure_allowed_evade();
StrategyEntry entry_119_vas_walk_evade();
StrategyEntry entry_120_module_list_hide();
StrategyEntry entry_121_ret_addr_spoof();
StrategyEntry entry_122_norm_hash_evade();
StrategyEntry entry_123_handle_inherit_detect();
StrategyEntry entry_107_polymorphic_build();
StrategyEntry entry_108_anti_re_canary();
StrategyEntry entry_109_build_watermark();

// Wave 7 forward declarations (IDs 124-147)
StrategyEntry entry_124_handle_hijack_proxy_donor();
StrategyEntry entry_125_wda_exclude_capture();
StrategyEntry entry_126_dxgi_output_duplication();
StrategyEntry entry_127_hud_radar_parsing();
StrategyEntry entry_128_cvar_walk_resolve();
StrategyEntry entry_129_pattern_scan_offsets();
StrategyEntry entry_130_batch_read_obfuscation();
StrategyEntry entry_131_temporal_phase_evasion();
StrategyEntry entry_132_decoy_render_ml_evasion();
StrategyEntry entry_133_dynamic_api_table();
StrategyEntry entry_134_stack_spoof_syscall();
StrategyEntry entry_135_callback_shadow_kernel();
StrategyEntry entry_136_ept_memory_hiding();
StrategyEntry entry_137_dma_page_table_walk();
StrategyEntry entry_138_shellcode_inject_donor();
StrategyEntry entry_139_helper_ticket_protocol();
StrategyEntry entry_140_runtime_health_ladder();
StrategyEntry entry_141_accept_readiness_gate();
StrategyEntry entry_142_hw_monitor_disguise();
StrategyEntry entry_143_forensic_cleanup_exit();
StrategyEntry entry_144_peb_spoof_hide();
StrategyEntry entry_145_page_oscillation_mem();
StrategyEntry entry_146_normalized_hash_evade();
StrategyEntry entry_147_vas_walk_evade_phase();
// Crosscutting strategy pair entries
StrategyEntry entry_148_adaptive_strategy();
StrategyEntry entry_149_behavioral_decoupler();
// UEFI firmware strategy entries
StrategyEntry entry_150_uefi_secureboot_bypass();
StrategyEntry entry_151_uefi_dma_bypass();
StrategyEntry entry_152_uefi_txt_measured_boot();
StrategyEntry entry_153_pe_timestamp_clone();

// Wave 8: remaining tier and crosscutting strategy pairs (IDs 29-99)
StrategyEntry entry_29_process_cooccurrence();
StrategyEntry entry_31_parent_lineage();
StrategyEntry entry_32_inmatch_only();
StrategyEntry entry_33_driver_allowlist();
StrategyEntry entry_34_scm_service();
StrategyEntry entry_35_callback_shadow();
StrategyEntry entry_36_nested_hv();
StrategyEntry entry_37_attestation();
StrategyEntry entry_38_timing_spoof();
StrategyEntry entry_39_capture_cv_hid();
StrategyEntry entry_40_entity_stream_crypto();
StrategyEntry entry_41_ac_self_integrity();
StrategyEntry entry_42_handle_hide_on_enum();
StrategyEntry entry_43_section_map();
StrategyEntry entry_44_stack_spoof();
StrategyEntry entry_45_etw_blind();
StrategyEntry entry_46_early_load_race();
StrategyEntry entry_47_object_callback_strip();
StrategyEntry entry_48_ept_hide_ac();
StrategyEntry entry_49_efi_boot_entry();
StrategyEntry entry_50_dual_boot_posture();
StrategyEntry entry_51_report_velocity();
StrategyEntry entry_53_thread_hijack();
StrategyEntry entry_54_module_stomp();
StrategyEntry entry_55_process_hollow();
StrategyEntry entry_56_minifilter_strip();
StrategyEntry entry_57_registry_notify_strip();
StrategyEntry entry_58_elam_bypass();
StrategyEntry entry_59_cr3_stealth_target();
StrategyEntry entry_60_aim_challenge();
StrategyEntry entry_62_silent_aim_desync();
StrategyEntry entry_63_veh_exception_cf();
StrategyEntry entry_64_iat_eat_hook();
StrategyEntry entry_65_dxgi_present_hook();
StrategyEntry entry_66_mapper_artifact();
StrategyEntry entry_67_dse_testsign();
StrategyEntry entry_68_dkom_hide();
StrategyEntry entry_69_physmem_map();
StrategyEntry entry_70_secure_kernel_view();
StrategyEntry entry_71_hvci_race();
StrategyEntry entry_72_desktop_duplication();
StrategyEntry entry_74_overwatch_queue();
StrategyEntry entry_76_gdi_bitblt();
StrategyEntry entry_77_wh_mouse_hook();
StrategyEntry entry_78_thread_hide_dbg();
StrategyEntry entry_79_speedhack_timescale();
StrategyEntry entry_80_pool_tag_hide();
StrategyEntry entry_81_wfp_ndis_filter();
StrategyEntry entry_82_ci_options();
StrategyEntry entry_83_feature_control_msr();
StrategyEntry entry_85_external_clone_display();
StrategyEntry entry_86_vpn_proxy_graph();
StrategyEntry entry_88_printwindow();
StrategyEntry entry_89_block_input();
StrategyEntry entry_90_sedebug_priv();
StrategyEntry entry_91_raw_vs_sendinput();
StrategyEntry entry_92_instr_callback();
StrategyEntry entry_93_etw_ti_blind();
StrategyEntry entry_94_infinity_hook();
StrategyEntry entry_95_vtl1_enclave_miss();
StrategyEntry entry_96_packet_loss_disambig();
StrategyEntry entry_97_clipcursor();
StrategyEntry entry_98_overwatch_multisignal();
StrategyEntry entry_99_clipboard_token();

// catalog: Return static vector of all registered StrategyEntry factories.
const std::vector<StrategyEntry>& catalog() {
  static const std::vector<StrategyEntry> k = {
      entry_01_external_rpm(),     entry_02_indirect_syscall(),
      entry_03_kernel_ioctl(),     entry_04_byovd(),
      entry_05_hypervisor(),       entry_06_dma_hardware(),
      entry_07_internal_inject(),  entry_08_manual_map_hide(),
      entry_09_overlay_esp(),      entry_10_web_phone_radar(),
       entry_11_aim_humanization(), entry_111_staged_loader(),
       entry_106_obfuscation_crypto(), entry_14_offset_c2(),
       entry_110_hwid_spoof(),      entry_16_callback_strip(),
      entry_17_handle_minimize(),  entry_18_read_throttle(),
      entry_19_input_synthesis(),  entry_20_fallback_chain(),
      entry_21_module_integrity(), entry_22_input_provenance(),
      entry_23_account_graph(),    entry_24_interest_mgmt(),
      entry_25_info_advantage(),   entry_26_delayed_ban(),
      entry_27_boot_trust(),       entry_28_network_c2_intel(),
      entry_29_pattern_offset_scan(),      entry_30_fp_allowlist_evasion(),
      entry_31_composition_radar_loop(),   entry_32_cs_round_radar(),
      entry_33_handle_hijack_proxy(),      entry_34_bomb_round_intel(),
      entry_35_spectator_feed(),  entry_36_triggerbot_timing(),
      entry_37_rcs_pattern(),     entry_38_sound_esp(),
      entry_39_streamproof_overlay(),      entry_40_no_flash(),
      entry_41_projectile_nade_esp(),      entry_42_object_glow_product(),
      entry_43_plant_defuse_hud(),         entry_44_internal_footprint(),
      entry_45_protector_suite(),          entry_46_fov_viewmodel_mod(),
      entry_47_noscope_inaccuracy_viz(),   entry_48_sound_viz_subtypes(),
      entry_49_crosshair_helper(),         entry_50_desktop_dup_capture(),
      entry_51_dll_thread_protect(),       entry_52_schema_saas_product(),
        entry_53_multimap_radar_share(),   entry_54_lag_switch(),
        entry_55_network_multibox_aim(),   entry_56_iommu_policy(),
        entry_57_windowless_swapchain(),   entry_58_heavens_gate_syscall(),
        entry_59_enhanced_stack_spoof(),   entry_60_physmem_direct_read(),
        entry_61_dkom_token_steal(),       entry_62_smm_read_channel(),
        entry_63_fpga_smart_dma(),         entry_64_scattered_entity_read(),
        entry_65_multi_process_ipc_split(), entry_66_dynamic_ssn_resolve(),
        entry_67_ntdll_hook_evade(),        entry_68_acpi_pm_mem_read(),
        entry_69_hypercall_mem_read(),      entry_70_ept_violation_evade(),
        entry_71_vmexit_keylog_capture(),   entry_72_pcie_peer_dma(),
         entry_100_skin_changer(),          entry_101_gaming_chair(),
         entry_102_vac_handle_enum(),       entry_103_thread_monitor_evade(),
         entry_104_vmt_integrity(),         entry_105_diagnostic_telemetry(),
          entry_112_vmt_proxy_evade(),      entry_113_self_obfuscate_process(),
          entry_114_proxy_hijack_reader(),
          entry_115_convars_temp_restore(),
          entry_116_etw_dual_provider(),
          entry_117_thread_4kb_signature(),
          entry_118_bsecure_allowed_evade(),
          entry_119_vas_walk_evade(),
          entry_120_module_list_hide(),
          entry_121_ret_addr_spoof(),
          entry_122_norm_hash_evade(),
          entry_123_handle_inherit_detect(),
          entry_107_polymorphic_build(),    entry_108_anti_re_canary(),
          entry_109_build_watermark(),
          // Wave 7: 24 new strategy pairs (IDs 124-147)
          entry_124_handle_hijack_proxy_donor(),    entry_125_wda_exclude_capture(),
          entry_126_dxgi_output_duplication(),      entry_127_hud_radar_parsing(),
          entry_128_cvar_walk_resolve(),            entry_129_pattern_scan_offsets(),
          entry_130_batch_read_obfuscation(),       entry_131_temporal_phase_evasion(),
          entry_132_decoy_render_ml_evasion(),      entry_133_dynamic_api_table(),
          entry_134_stack_spoof_syscall(),          entry_135_callback_shadow_kernel(),
          entry_136_ept_memory_hiding(),            entry_137_dma_page_table_walk(),
          entry_138_shellcode_inject_donor(),       entry_139_helper_ticket_protocol(),
          entry_140_runtime_health_ladder(),        entry_141_accept_readiness_gate(),
          entry_142_hw_monitor_disguise(),          entry_143_forensic_cleanup_exit(),
          entry_144_peb_spoof_hide(),               entry_145_page_oscillation_mem(),
           entry_146_normalized_hash_evade(),        entry_147_vas_walk_evade_phase(),
           // Crosscutting strategy pairs
           entry_148_adaptive_strategy(),            entry_149_behavioral_decoupler(),
           // UEFI firmware strategies
           entry_150_uefi_secureboot_bypass(),       entry_151_uefi_dma_bypass(),
           entry_152_uefi_txt_measured_boot(),        entry_153_pe_timestamp_clone(),
           // Wave 8: remaining tier and crosscutting strategy pairs
           entry_29_process_cooccurrence(),          entry_31_parent_lineage(),
           entry_32_inmatch_only(),                  entry_33_driver_allowlist(),
           entry_34_scm_service(),                   entry_35_callback_shadow(),
           entry_36_nested_hv(),                     entry_37_attestation(),
           entry_38_timing_spoof(),                  entry_39_capture_cv_hid(),
           entry_40_entity_stream_crypto(),          entry_41_ac_self_integrity(),
           entry_42_handle_hide_on_enum(),           entry_43_section_map(),
           entry_44_stack_spoof(),                   entry_45_etw_blind(),
           entry_46_early_load_race(),               entry_47_object_callback_strip(),
           entry_48_ept_hide_ac(),                   entry_49_efi_boot_entry(),
           entry_50_dual_boot_posture(),             entry_51_report_velocity(),
           entry_53_thread_hijack(),                 entry_54_module_stomp(),
           entry_55_process_hollow(),                entry_56_minifilter_strip(),
           entry_57_registry_notify_strip(),         entry_58_elam_bypass(),
           entry_59_cr3_stealth_target(),            entry_60_aim_challenge(),
           entry_62_silent_aim_desync(),             entry_63_veh_exception_cf(),
           entry_64_iat_eat_hook(),                  entry_65_dxgi_present_hook(),
           entry_66_mapper_artifact(),               entry_67_dse_testsign(),
           entry_68_dkom_hide(),                     entry_69_physmem_map(),
           entry_70_secure_kernel_view(),            entry_71_hvci_race(),
           entry_72_desktop_duplication(),           entry_74_overwatch_queue(),
           entry_76_gdi_bitblt(),                    entry_77_wh_mouse_hook(),
           entry_78_thread_hide_dbg(),               entry_79_speedhack_timescale(),
           entry_80_pool_tag_hide(),                 entry_81_wfp_ndis_filter(),
           entry_82_ci_options(),                    entry_83_feature_control_msr(),
           entry_85_external_clone_display(),        entry_86_vpn_proxy_graph(),
           entry_88_printwindow(),                   entry_89_block_input(),
           entry_90_sedebug_priv(),                  entry_91_raw_vs_sendinput(),
           entry_92_instr_callback(),                entry_93_etw_ti_blind(),
           entry_94_infinity_hook(),                 entry_95_vtl1_enclave_miss(),
           entry_96_packet_loss_disambig(),          entry_97_clipcursor(),
           entry_98_overwatch_multisignal(),         entry_99_clipboard_token(),
    };
  return k;
}

// find: Lookup a StrategyEntry by meta.id, or nullptr.
const StrategyEntry* find(std::string_view id) {
  for (const auto& e : catalog()) {
    if (id == e.meta.id) return &e;
  }
  return nullptr;
}

}  // namespace strategies
