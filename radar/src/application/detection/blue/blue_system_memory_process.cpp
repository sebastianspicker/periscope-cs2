#include "blue/blue_system.hpp"
#include "blue/blue_system_internal.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace blue {
using namespace blue_detail;

BlueViewResult BlueCoordinator::check_memory_pattern() {
  BlueViewResult result{ac::ObservationView::MemoryPattern, false, 0.0, "ok", {}};

  // Classic external RPM volume thresholds (lab defaults).
  if (world_.remote_read_ops > 100) {
    add_reason(result,
               "high_rpm_ops=" + std::to_string(world_.remote_read_ops),
               0.45 + std::min(0.35, world_.remote_read_ops / 1000.0));
  } else if (world_.remote_read_ops > 40) {
    add_reason(result,
               "elevated_rpm_ops=" + std::to_string(world_.remote_read_ops), 0.30);
  }

  if (world_.remote_read_bytes > 1024u * 1024u) {
    add_reason(result,
               "high_read_volume_bytes=" + std::to_string(world_.remote_read_bytes),
               0.40);
  } else if (world_.remote_read_bytes > 256u * 1024u) {
    add_reason(result,
               "elevated_read_bytes=" + std::to_string(world_.remote_read_bytes),
               0.25);
  }

  if (world_.scattered_read_pattern || world_.scattered_read_count > 20) {
    add_reason(result,
               "scattered_reads count=" + std::to_string(world_.scattered_read_count),
               0.40);
  }
  if (world_.batch_read_obfuscated ||
      (world_.batch_read_count > 0 && world_.batch_read_shuffled)) {
    add_reason(result,
               "batch_read_obfuscation count=" + std::to_string(world_.batch_read_count),
               0.35);
  }
  if (world_.stack_spoof_on_read || world_.enhanced_stack_spoof ||
      world_.stack_spoof_syscall_active) {
    add_reason(result, "stack_spoof_on_read", 0.35);
  }
  if (world_.physmem_device_open || world_.physmem_direct_mapped) {
    add_reason(result, "physmem_path", 0.55);
  }
  if (world_.fpga_smart_dma_active || world_.trust.dma_device_present) {
    const int scat = world_.fpga_scatter_reads;
    add_reason(result, "dma_device_or_fpga scatter=" + std::to_string(scat), 0.50);
  }
  if (world_.iommu_bypass_active || world_.iommu_bypass_confirmed ||
      world_.iommu_disabled || world_.iommu_state == sim::IommuState::Bypassed) {
    add_reason(result, "iommu_bypass_or_disabled", 0.45);
  }
  if (world_.pcie_peer_dma_active || world_.thunderbolt_dma_active ||
      world_.usb_dfu_dma_active) {
    add_reason(result, "pcie_or_tb_dma_channel", 0.50);
  }
  if (world_.dma_page_table_walked) {
    add_reason(result,
               "dma_page_walk levels=" + std::to_string(world_.dma_page_levels_walked),
               0.45);
  }
  if (world_.smm_read_channel_planted || world_.acpi_pm_read_active ||
      world_.hypercall_read_active) {
    add_reason(result, "firmware_or_hv_read_channel", 0.50);
  }
  if (world_.schema_remote_update || world_.schema_saas_product) {
    add_reason(result,
               "schema_remote_product fetches=" +
                   std::to_string(world_.schema_fetch_count),
               0.30);
  }
  // Pattern rescan as memory-side residual (also scored in PostExecution).
  if (world_.pattern_rescan_count > 3) {
    add_reason(result,
               "pattern_rescan_count=" + std::to_string(world_.pattern_rescan_count),
               0.25);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::MemoryPattern), 0.40);
  return result;
}

// ── InProcess: hooks, threads, VEH, overlay present-path integrity ─────────

BlueViewResult BlueCoordinator::check_in_process() {
  BlueViewResult result{ac::ObservationView::InProcess, false, 0.0, "ok", {}};
  const auto* game = world_.proc(world_.game_pid());

  if (game != nullptr) {
    if (game->has_foreign_thread) {
      add_reason(result, "foreign_thread_in_game", 0.55);
    }
    if (game->thread_hijacked) {
      add_reason(result, "thread_hijacked", 0.55);
    }
    for (const auto& module : game->modules) {
      if (module.present_hooked) {
        add_reason(result, "present_hook module=" + module.name, 0.55);
        break;
      }
      if (module.iat_hooked || module.eat_hooked) {
        add_reason(result, "inproc_api_hook module=" + module.name, 0.45);
        break;
      }
    }
  }

  if (world_.apc_injection_active || world_.apc_injection_count > 0) {
    add_reason(result,
               "apc_injection count=" + std::to_string(world_.apc_injection_count),
               0.50);
  }
  if (world_.shellcode_donor_active) {
    add_reason(result,
               "shellcode_donor pid=" + std::to_string(world_.shellcode_donor_pid),
               0.50);
  }
  if (world_.steam_present_hooked || world_.steam_trampoline_hooks_written ||
      world_.steam_resize_hooked || world_.steam_resize_buffers_hooked) {
    add_reason(result, "steam_overlay_present_hook", 0.50);
  }
  if (world_.windowless_swapchain_hijack || world_.swapchain_hijacked_no_window ||
      world_.vtable_present_hook_fallback) {
    add_reason(result, "swapchain_hijack", 0.50);
  }
  if (world_.cross_process_hijack || world_.cross_process_swapchain) {
    add_reason(result, "cross_process_present", 0.45);
  }
  for (const auto& ov : world_.overlays) {
    if (ov.steam_overlay_hijacked || ov.hijacks_swapchain ||
        ov.cross_process_hijack) {
      add_reason(result, "overlay_hijack title=" + ov.title, 0.45);
      break;
    }
  }
  if (!world_.veh_chain_clean || world_.veh_cf_patched ||
      world_.veh_handlers_modified > 0) {
    add_reason(result,
               "veh_tamper handlers_mod=" +
                   std::to_string(world_.veh_handlers_modified),
               0.40);
  }
  if (world_.instrumentation_callback) {
    add_reason(result, "instrumentation_callback", 0.40);
  }
  if (world_.infinity_hook_residual) {
    add_reason(result, "infinity_hook_residual", 0.45);
  }
  if (world_.ntdll_hooks_detected && world_.used_clean_ntdll_copy) {
    add_reason(result, "clean_ntdll_copy_evasion", 0.35);
  }
  if (world_.hw_breakpoint_evasion_active || world_.debug_registers_cleared) {
    add_reason(result, "hwbp_evasion", 0.30);
  }
  if (world_.thread_hide_from_debugger || world_.peb_being_debugged_spoofed ||
      world_.peb_spoof_active) {
    add_reason(result, "anti_debug_inproc", 0.35);
  }
  if (world_.red_hiding_behind_self_patch) {
    add_reason(result, "self_patch_hide", 0.40);
  }
  if (world_.diagnostic_state.anomalous_vmt ||
      world_.diagnostic_state.thread_start_obfuscated) {
    add_reason(result, "diag_vmt_or_thread_start_anomaly", 0.45);
  }
  if (world_.diagnostic_state.thread_capture.from_suspicious_module ||
      world_.diagnostic_state.thread_capture.from_rwx_memory) {
    add_reason(result, "diag_suspicious_thread_start", 0.50);
  }
  for (const auto& ex : world_.diagnostic_state.exceptions) {
    if (ex.contains_cheat_frames) {
      add_reason(result, "exception_cheat_frames", 0.45);
      break;
    }
  }
  // Kernel callback integrity (in-process AC vantage residual).
  if (world_.callback_shadow_active || world_.callback_kernel_shadow_active ||
      !world_.ac_callback_present) {
    add_reason(result, "callback_shadow_or_missing_ac", 0.40);
  }
  if (world_.object_callbacks_present == false &&
      world_.object_callbacks_true_present) {
    add_reason(result, "object_callbacks_stripped", 0.40);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::InProcess), 0.40);
  return result;
}

// ── Behavioral: aim / input / counter-strafe / info-advantage residuals ────


}  // namespace blue
