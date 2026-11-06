#include "blue/blue_system.hpp"
#include "blue/blue_system_internal.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace blue {
using namespace blue_detail;

BlueViewResult BlueCoordinator::check_behavioral() {
  BlueViewResult result{ac::ObservationView::Behavioral, false, 0.0, "ok", {}};

  // Counter-strafe perfect-frame residual (CS2 diagnostic path).
  const auto& events = world_.diagnostic_state.counter_strafe_events;
  if (events.size() > 8) {
    int perfect = 0;
    int samples = 0;
    for (std::size_t i = 1; i < events.size(); ++i) {
      const auto delta = static_cast<std::int64_t>(events[i].tick) -
                         static_cast<std::int64_t>(events[i - 1].tick);
      ++samples;
      if (delta == 1) {
        ++perfect;
      }
    }
    if (samples > 0 && perfect > samples * 0.5) {
      add_reason(result,
                 "counter_strafe_perfect " + std::to_string(perfect) + "/" +
                     std::to_string(samples),
                 0.55);
    }
  }
  if (world_.cs_total_actions > 10 && world_.cs_perfect_frames > 0) {
    const double ratio =
        static_cast<double>(world_.cs_perfect_frames) /
        static_cast<double>(std::max(1, world_.cs_total_actions));
    if (ratio > 0.4) {
      add_reason(result, "cs_perfect_frame_ratio", 0.45);
    }
  }

  // Aim residual samples.
  int silent_desync = 0;
  int hidden_aim = 0;
  int trigger_fast = 0;
  int rcs_track = 0;
  for (const auto& a : world_.aim_samples) {
    const float dy = std::fabs(a.camera_yaw - a.server_aim_yaw);
    const float dp = std::fabs(a.camera_pitch - a.server_aim_pitch);
    if (dy > 2.0f || dp > 2.0f || !a.challenge_passed) {
      ++silent_desync;
    }
    if (a.aim_on_hidden_target && !a.has_vision_on_target && !a.has_audio_on_target) {
      ++hidden_aim;
    }
    if (a.crosshair_on_enemy && a.fired && a.fire_latency_ms > 0.f &&
        a.fire_latency_ms < 15.f) {
      ++trigger_fast;
    }
    if (a.recoil_expected_pitch != 0.f &&
        std::fabs(a.rcs_applied_pitch - a.recoil_expected_pitch) < 0.05f) {
      ++rcs_track;
    }
  }
  if (world_.silent_aim_active || silent_desync >= 3) {
    add_reason(result,
               "silent_aim_desync samples=" + std::to_string(silent_desync), 0.55);
  }
  if (hidden_aim >= 2) {
    add_reason(result, "aim_without_vision_or_audio n=" + std::to_string(hidden_aim),
               0.55);
  }
  if (world_.triggerbot_active || trigger_fast >= 3 ||
      (world_.trigger_on_target_fires >= 5 && world_.trigger_mean_latency_ms < 20.f)) {
    add_reason(result,
               "triggerbot residual fires=" +
                   std::to_string(world_.trigger_on_target_fires),
               0.50);
  }
  if (world_.rcs_active || rcs_track >= 5) {
    add_reason(result, "rcs_pattern_tracking", 0.40);
  }
  if (world_.sound_esp_active || world_.sound_inferred_without_los >= 3 ||
      world_.sound_viz_subtypes_active) {
    add_reason(result, "sound_esp_or_viz", 0.45);
  }
  if (world_.bomb_intel_product || world_.projectile_esp_active ||
      world_.object_glow_product || world_.nade_prediction_active) {
    add_reason(result, "info_product_esp", 0.40);
  }
  if (world_.no_flash_active && world_.flash_alpha_forced < 0.2f) {
    add_reason(result, "no_flash_fx_strip", 0.35);
  }
  if (world_.fov_mod_active && world_.client_fov_override > 0.f) {
    add_reason(result, "fov_mod", 0.25);
  }
  if (world_.time_scale != 1.0 &&
      (world_.time_scale < 0.95 || world_.time_scale > 1.05)) {
    add_reason(result, "speedhack_time_scale=" + std::to_string(world_.time_scale),
               0.50);
  }
  if (world_.guest_time_dilation_active) {
    add_reason(result, "guest_time_dilation", 0.40);
  }
  if (world_.multibox_net_aim || world_.multibox_input_desync ||
      world_.multibox_aim_samples > 5) {
    add_reason(result, "multibox_aim_stream", 0.45);
  }
  if (world_.raw_sendinput_mixed) {
    add_reason(result, "raw_sendinput_mixed", 0.35);
  }
  // Injected input residual.
  int injected = 0;
  for (const auto& in : world_.inputs) {
    if (in.source == "injected" || in.source == "serial_arduino" ||
        in.source == "kmbox") {
      ++injected;
    }
  }
  if (injected >= 5) {
    add_reason(result, "injected_input_events=" + std::to_string(injected), 0.40);
  }
  if (world_.decoy_render_active && world_.ml_confusion_active) {
    add_reason(result, "decoy_render_ml_confusion", 0.30);
  }
  if (world_.lag_switch_active || world_.packet_loss_faked) {
    add_reason(result, "lag_switch_or_faked_loss", 0.35);
  }
  if (world_.spectator_feed_active && world_.spectator_has_delayed_enemy_origin) {
    add_reason(result, "spectator_delayed_origin_feed", 0.35);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::Behavioral), 0.40);
  return result;
}

// ── PostExecution: forensic cleanup, rescans, delayed ban, disk residuals ──

BlueViewResult BlueCoordinator::check_post_execution() {
  BlueViewResult result{ac::ObservationView::PostExecution, false, 0.0, "ok", {}};

  if (world_.pattern_rescan_count > 5) {
    add_reason(result,
               "pattern_rescan_artifacts=" +
                   std::to_string(world_.pattern_rescan_count),
               0.40);
  } else if (world_.pattern_rescan_count > 2) {
    add_reason(result,
               "pattern_rescan_count=" + std::to_string(world_.pattern_rescan_count),
               0.25);
  }

  // Forensic cleanup itself is a post-exec residual (red tried to scrub).
  if (world_.forensic_cleanup_active || world_.forensic_prefetch_cleared ||
      world_.forensic_recent_cleared || world_.forensic_cleanup_steps_completed > 0) {
    add_reason(result,
               "forensic_cleanup steps=" +
                   std::to_string(world_.forensic_cleanup_steps_completed),
               0.50);
  }
  if (world_.registry_persisted_config) {
    add_reason(result, "registry_persist key=" + world_.registry_key_path, 0.40);
  }
  if (world_.clipboard_token_leak) {
    add_reason(result, "clipboard_token_leak", 0.35);
  }
  if (world_.lab_delayed_ban_ready || world_.overwatch_queued) {
    add_reason(result, "delayed_ban_or_overwatch_queued", 0.45);
  }
  if (world_.overwatch_score >= 3.0) {
    add_reason(result,
               "overwatch_score=" + std::to_string(world_.overwatch_score), 0.40);
  }
  if (world_.lab_confidence >= 2.0) {
    add_reason(result,
               "lab_confidence_accum=" + std::to_string(world_.lab_confidence), 0.35);
  }
  if (!world_.build_watermark.empty() ||
      (world_.binary_build_id != "shared" && !world_.binary_build_id.empty())) {
    add_reason(result, "build_watermark_or_poly_id", 0.30);
  }
  if (world_.binary_padding_applied) {
    add_reason(result,
               "binary_padding size=" + std::to_string(world_.padded_binary_size),
               0.25);
  }
  if (world_.self_destruct_armed || world_.protector_watchdog) {
    add_reason(result, "protector_self_destruct_or_watchdog", 0.35);
  }
  if (world_.normalized_hash_evade_active || world_.pe_hash_normalized) {
    add_reason(result, "normalized_pe_hash_evasion", 0.35);
  }
  if (world_.trust.attestation_valid == false ||
      world_.trust.attestation_pcr_ok == false || world_.pcr_value_tampered ||
      world_.tpm_measurement_spoofed) {
    add_reason(result, "attestation_or_pcr_fail", 0.50);
  }
  if (world_.trust.unexpected_efi_entry || world_.trust.dual_boot_profile ||
      world_.trust.measured_launch_hidden) {
    add_reason(result, "efi_or_measured_boot_residual", 0.40);
  }
  if (world_.ci_options_disabled || world_.trust.test_signing ||
      !world_.trust.dse_enforced) {
    add_reason(result, "dse_or_ci_weakened", 0.40);
  }
  // Prefetch / recent artifacts via service events or notes are weaker.
  if (world_.canary_tripped) {
    add_reason(result, "analysis_canary_tripped", 0.30);
  }
  if (world_.stealth_exfiltration_active && world_.stealth_exfiltrated_bytes > 0) {
    add_reason(result,
               "stealth_exfil_bytes=" +
                   std::to_string(world_.stealth_exfiltrated_bytes),
               0.35);
  }
  // Net C2 residual as post-session forensic.
  int c2_flows = 0;
  for (const auto& n : world_.net) {
    if (n.looks_like_offset_c2 || n.looks_like_radar_saas) {
      ++c2_flows;
    }
  }
  if (c2_flows >= 1 || world_.c2_domain_fronting_active ||
      world_.c2_fallback_chain_active) {
    add_reason(result, "c2_or_radar_saas_net residual", 0.40);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::PostExecution), 0.40);
  return result;
}

}  // namespace blue
