#pragma once

// Advanced T0 evasion composition for the sim-only anti-cheat lesson.
//
// NOTE: These methods set sim::World flags only. For production evasion
// (TemporalEngine, DecoyRenderEngine, BatchEngine, etc.), compose the
// application pipeline that instantiates the simulation evasion engines.

#include "t0_red/cheat_client.hpp"

#include <string>
#include <vector>

namespace t0_red {

struct AdvancedEvasionReport {
  bool staged_loader = false;
  bool multi_process_split = false;
  bool handle_proxy = false;
  bool windowless_swapchain = false;
  bool scattered_reads = false;
  bool entity_stream_crypto = false;
  bool process_hollowing = false;
  bool apc_injection = false;
  bool named_pipe_ipc = false;
  bool registry_config = false;
  bool binary_padding = false;
  bool steam_overlay_hijack_active = false;
  bool window_hijack_active = false;
  bool hidden_from_weak_enum = false;
  bool throttled = false;

  // Enhanced evasion flags
  bool temporal_jitter_active = false;
  bool batch_shuffle_active = false;
  bool memory_hide_active = false;
  bool decoy_render_active = false;

  CheatClientReport client_report{};
  HandleGraphMonitorProbe blue_sensor_probe{};
  std::vector<std::string> steps;
  std::string detail;
};

class AdvancedEvasionKit {
 public:
  AdvancedEvasionKit(sim::World& world, CheatClient& client)
      : world_(world), client_(client) {}

  /// Apply the complete simulated stack and describe its remaining blue scars.
  AdvancedEvasionReport max_stealth();
  /// Apply every T0 simulation technique, including the Wave 11 scar surface.
  AdvancedEvasionReport deep_stealth();
  /// Compose Wave 13 overlay simulations with every existing advanced technique.
  AdvancedEvasionReport deep_stealth_v2();

  // ── Enhanced evasion methods ─────────────────────────────────

  /// Add temporal jitter to read timing (random delays).
  bool enable_temporal_jitter(int min_ms = 1, int max_ms = 16);

  /// Batch shuffle: Fisher-Yates shuffle of read order.
  bool enable_batch_shuffle();

  /// Memory hide: unlink from PEB or hide mapped regions.
  bool enable_memory_hide();

  /// Decoy render: create fake overlay windows to confuse blue.
  bool enable_decoy_render(int count = 3);

 private:
  sim::World& world_;
  CheatClient& client_;
};

}  // namespace t0_red
