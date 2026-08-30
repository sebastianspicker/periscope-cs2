#include "sim/evasion_master.hpp"

#include <cmath>
#include <limits>
#include <sstream>

namespace sim {

EvasionMaster::EvasionMaster(World& world) : world_(world) {}

void EvasionMaster::init(const EvasionConfig& config, std::uint32_t cheat_pid) {
  config_ = config;
  state_ = EvasionState{};
  rng_.seed(config_.replay_seed);
  cheat_pid_ = cheat_pid;
  populate_defense_layers();

  world_.scattered_read_pattern = config_.read_order_shuffle;
  world_.read_timing_jitter = config_.pattern_timing_jitter;
  world_.batch_read_shuffled = config_.read_order_shuffle;
  world_.batch_read_jittered = config_.pattern_timing_jitter;

  if (config_.disguise_enabled) {
    apply_disguise(cheat_pid);
  }
  if (config_.peb_unlink_enabled || config_.working_set_trim_enabled ||
      config_.thread_hide_enabled) {
    apply_memory_hide(cheat_pid);
  }
  if (config_.temporal_phase_enabled) {
    world_.temporal_phase_active = true;
    world_.temporal_phase_index = 0;
    world_.opsec_pattern_count = config_.temporal_phase_count;
    world_.multi_tick_opsec_active = true;
  }
  if (config_.decoy_read_ratio > 0.0) {
    world_.decoy_render_active = true;
  }

  world_.note("EvasionMaster initialized for pid " + std::to_string(cheat_pid));
}

std::vector<ac::EntitySnapshot> EvasionMaster::tick(
    std::vector<ac::EntitySnapshot> entities, std::uint64_t current_tick,
    std::uint32_t cheat_pid) {
  (void)current_tick;
  if (cheat_pid != 0) {
    cheat_pid_ = cheat_pid;
  }
  state_.tick_count++;

  if (config_.temporal_phase_enabled) {
    advance_temporal_phase();
  }

  if (should_decoy_read()) {
    state_.decoy_reads++;
    world_.ml_decoy_patterns_generated =
        static_cast<int>(state_.decoy_reads);
  }

  if (should_drop_frame()) {
    state_.frames_dropped++;
    return entities;
  }
  state_.frames_rendered++;

  const double latency = sample_latency_ms();
  (void)latency;

  std::vector<ac::EntitySnapshot> filtered;
  filtered.reserve(entities.size());
  for (auto& entity : entities) {
    if (should_omit_entity(entity.id)) {
      state_.entities_omitted++;
      continue;
    }
    const auto fuzzed = fuzz_position(entity.origin);
    const auto dx = static_cast<double>(fuzzed.x) -
                    static_cast<double>(entity.origin.x);
    const auto dy = static_cast<double>(fuzzed.y) -
                    static_cast<double>(entity.origin.y);
    state_.total_fuzz_applied_px += std::sqrt(dx * dx + dy * dy);
    entity.origin = fuzzed;
    filtered.push_back(entity);
  }

  if (config_.read_order_shuffle && filtered.size() > 1) {
    filtered = shuffle_entities(std::move(filtered));
    world_.scattered_read_count +=
        static_cast<int>(std::min(filtered.size(),
                                  static_cast<std::size_t>(
                                      std::numeric_limits<int>::max())));
    world_.batch_read_obfuscated = true;
    world_.batch_read_count += static_cast<int>(filtered.size());
  }

  // Proxy rotation residual: every N ticks, mark proxy rotation scar.
  if (config_.proxy_rotation_interval > 0 &&
      state_.tick_count %
              static_cast<std::uint64_t>(config_.proxy_rotation_interval) ==
          0) {
    world_.handle_proxy_active = world_.handle_proxy_active ||
                                 world_.handle_proxy_owner_pid != 0;
    world_.note("evasion proxy_rotation tick=" +
                std::to_string(state_.tick_count));
  }

  return filtered;
}

void EvasionMaster::shutdown(std::uint32_t cheat_pid) {
  if (config_.forensic_clean_enabled) {
    apply_forensic_clean(cheat_pid);
  }
  world_.scattered_read_pattern = false;
  world_.read_timing_jitter = false;
  world_.temporal_phase_active = false;
  world_.note("EvasionMaster shutdown for pid " + std::to_string(cheat_pid));
}

void EvasionMaster::apply_memory_hide(std::uint32_t cheat_pid) {
  auto* cheat = world_.proc(cheat_pid);
  if (cheat == nullptr) {
    return;
  }
  if (config_.peb_unlink_enabled) {
    for (auto& m : cheat->modules) {
      m.linked_in_peb = false;
    }
    world_.peb_spoof_active = true;
    world_.peb_being_debugged_cleared = true;
    world_.peb_nt_global_flag_cleared = true;
    world_.peb_being_debugged_spoofed = true;
    cheat->peb_identity_matched = true;
    state_.peb_unlinked = true;
  }
  if (config_.working_set_trim_enabled) {
    cheat->working_set_kb = 0;
    state_.working_set_trimmed = true;
  }
  if (config_.thread_hide_enabled) {
    world_.thread_hide_from_debugger = true;
    world_.thread_protection_active = true;
  }
  world_.internal_footprint_stealth = true;
  world_.note("evasion memory_hide pid=" + std::to_string(cheat_pid));
}

void EvasionMaster::apply_forensic_clean(std::uint32_t cheat_pid) {
  world_.forensic_cleanup_active = true;
  world_.forensic_prefetch_cleared = true;
  world_.forensic_recent_cleared = true;
  world_.forensic_cleanup_steps_completed = 3;
  state_.forensic_cleaned = true;
  world_.note("evasion forensic_clean pid=" + std::to_string(cheat_pid));
}

double EvasionMaster::sample_latency_ms() {
  if (config_.latency_ms_max <= config_.latency_ms_min) {
    state_.total_latency_ms += config_.latency_ms_min;
    return config_.latency_ms_min;
  }
  const double span = config_.latency_ms_max - config_.latency_ms_min;
  const double sample = config_.latency_ms_min + random_double() * span;
  state_.total_latency_ms += sample;
  return sample;
}

bool EvasionMaster::should_decoy_read() {
  return config_.decoy_read_ratio > 0.0 &&
         random_double() < config_.decoy_read_ratio;
}

bool EvasionMaster::should_drop_frame() {
  return config_.frame_drop_rate > 0.0 &&
         random_double() < config_.frame_drop_rate;
}

bool EvasionMaster::should_omit_entity(std::uint32_t entity_id) {
  (void)entity_id;
  return config_.entity_omission_rate > 0.0 &&
         random_double() < config_.entity_omission_rate;
}

ac::Vec3 EvasionMaster::fuzz_position(const ac::Vec3& pos) {
  if (config_.position_fuzz_px <= 0.0) {
    return pos;
  }

  ac::Vec3 fuzzed = pos;
  const auto random_offset = [this](double range) {
    return static_cast<float>((random_double() * 2.0 - 1.0) * range);
  };
  // Scale fuzz by temporal phase intensity (phase 0 mild → phase N-1 stronger).
  double phase_scale = 1.0;
  if (config_.temporal_phase_enabled && config_.temporal_phase_count > 0) {
    phase_scale = 0.5 + (static_cast<double>(state_.temporal_phase) /
                         static_cast<double>(config_.temporal_phase_count));
  }
  const double range = config_.position_fuzz_px * phase_scale;
  fuzzed.x += random_offset(range);
  fuzzed.y += random_offset(range);
  fuzzed.z += random_offset(range * 0.5);
  return fuzzed;
}

std::vector<ac::EntitySnapshot> EvasionMaster::shuffle_entities(
    std::vector<ac::EntitySnapshot> entities) {
  std::shuffle(entities.begin(), entities.end(), rng_);
  return entities;
}

void EvasionMaster::apply_disguise(std::uint32_t cheat_pid) {
  auto* cheat = world_.proc(cheat_pid);
  if (cheat == nullptr) {
    return;
  }

  switch (config_.active_disguise) {
    case ac::DisguiseProfile::RivaTuner:
      cheat->name = "RTSS.exe";
      cheat->window_class = "RivaTunerWndClass";
      cheat->looks_reputable = true;
      cheat->signer = "Unwinder";
      world_.hw_monitor_disguise_active = true;
      world_.hw_monitor_decoy_osd_active = true;
      world_.hw_monitor_rtss_hijack = true;
      break;
    case ac::DisguiseProfile::DiscordOverlay:
      cheat->name = "Discord.exe";
      cheat->window_class = "Chrome_WidgetWin_1";
      cheat->looks_reputable = true;
      cheat->signer = "Discord Inc.";
      break;
    case ac::DisguiseProfile::SteamOverlay:
      cheat->name = "gameoverlayui.exe";
      cheat->window_class = "SDL_app";
      cheat->looks_reputable = true;
      cheat->signer = "Valve Corp.";
      break;
    case ac::DisguiseProfile::ObsStudio:
      cheat->name = "obs64.exe";
      cheat->window_class = "OBSWindowClass";
      cheat->looks_reputable = true;
      cheat->signer = "OBS Project";
      break;
    case ac::DisguiseProfile::NvidiaShadowplay:
      cheat->name = "nvsphelper64.exe";
      cheat->window_class = "NvShadowPlay";
      cheat->looks_reputable = true;
      cheat->signer = "NVIDIA Corporation";
      break;
    case ac::DisguiseProfile::GenericMonitor:
      cheat->name = "MSIAfterburner.exe";
      cheat->window_class = "MSIAfterburner";
      cheat->looks_reputable = true;
      world_.hw_monitor_disguise_active = true;
      break;
    default:
      break;
  }
  state_.disguise_applied = true;
}

void EvasionMaster::populate_defense_layers() {
  state_.active_layers.clear();
  state_.active_layers.push_back(ac::DefenseLayer::ProcessIsolation);
  if (config_.disguise_enabled) {
    state_.active_layers.push_back(ac::DefenseLayer::HardwareMonitorDisguise);
  }
  if (config_.forensic_clean_enabled) {
    state_.active_layers.push_back(ac::DefenseLayer::ForensicTraceRemoval);
  }
  if (config_.jitter_variance > 0.0 || config_.frame_drop_rate > 0.0 ||
      config_.entity_omission_rate > 0.0 || config_.position_fuzz_px > 0.0 ||
      config_.pattern_timing_jitter) {
    state_.active_layers.push_back(ac::DefenseLayer::BehavioralJitter);
  }
  if (config_.decoy_read_ratio > 0.0 ||
      config_.active_disguise != ac::DisguiseProfile::None) {
    state_.active_layers.push_back(ac::DefenseLayer::SystemNormalization);
  }
}

void EvasionMaster::advance_temporal_phase() {
  if (config_.temporal_phase_count <= 0) {
    return;
  }
  // Cycle phase every ~32 ticks.
  if (state_.tick_count > 0 && (state_.tick_count % 32) == 0) {
    const int prev = state_.temporal_phase;
    state_.temporal_phase =
        (state_.temporal_phase + 1) % config_.temporal_phase_count;
    if (state_.temporal_phase != prev) {
      state_.phase_transitions++;
      world_.temporal_phase_transitions = state_.phase_transitions;
    }
  }
  world_.temporal_phase_index = state_.temporal_phase;
  world_.temporal_phase_ticks_in_phase =
      static_cast<int>(state_.tick_count % 32);
  world_.opsec_pattern_variant = state_.temporal_phase;
}

double EvasionMaster::random_double() {
  std::uniform_real_distribution<double> distribution(0.0, 1.0);
  return distribution(rng_);
}

}  // namespace sim
