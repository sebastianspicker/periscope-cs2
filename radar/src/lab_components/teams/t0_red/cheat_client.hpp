#pragma once

// Full T0 educational cheat client on sim::World only.
// Multi-step: spawn → OpenProcess(VM_READ) → RPM entity table → external radar.
// Enhanced with real backend paths: Sim, Rpm, Hijack.
// HijackReader wired for real CS2 reads via donor handle duplication.

#include "ac/types.hpp"
#include "sim/world.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t0_red/rpm_backend.hpp"


#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifndef OBF
#define OBF(value) value
#endif

namespace t0_red {

/// Which memory reading backend is in use.
enum class MemoryBackend {
  Sim,    // sim::World based (lab default)
  Rpm,    // ReadProcessMemory (classic T0)
  Hijack, // Donor handle hijack (stealth T0)
};

/// Auto-detect best available backend.
inline MemoryBackend auto_detect_backend() noexcept {
  return MemoryBackend::Sim;
}

// Lab type `Blip` used by this educational unit.
// x/y: map-relative (local-origin). screen_*: radar canvas pixels (origin top-left).
// world_*: absolute sim world X/Z projected as X/Y for tests and UI.
struct Blip {
  float x = 0, y = 0;
  float screen_x = 0, screen_y = 0;
  float world_x = 0, world_y = 0;
  std::uint8_t team = 0;
};

// Aggregate outcome fields for `CheatClientReport` (lab narrative / tests).
struct CheatClientReport {
  bool attached = false;
  bool entities_ok = false;
  bool radar_ok = false;
  int entity_count = 0;
  int blip_count = 0;
  int foreign_vm_read_handles = 0;
  std::uint64_t bytes_read = 0;
  int read_ops = 0;
  bool multi_process_split = false;
  bool handle_proxy = false;
  bool windowless_swapchain = false;
  bool scattered_reads = false;
  bool entity_stream_crypto = false;
  bool process_hollowing = false;
  std::uint32_t hollowed_pid = 0;
  bool apc_injection = false;
  bool named_pipe_ipc = false;
  bool registry_config = false;
  bool binary_padding = false;
  bool steam_overlay_hijack = false;
  bool window_hijack_overlay = false;
  bool steam_hook_present = false;
  bool steam_hook_resize = false;
  bool swapchain_pinned = false;
  int pin_wait_frames = 0;
  bool vtable_fallback = false;
  bool steam_critical_section = false;
  std::uint32_t handle_holder_pid = 0;
  std::uint32_t reader_pid = 0;
  std::uint32_t ui_pid = 0;
  std::string detail;
};

struct HandleGraphMonitorProbe {
  bool ac_process_present = false;
  bool ac_has_game_handle = false;
  bool blue_handle_sampling_likely = false;
  int observable_game_handles = 0;
  std::string detail;
};

/// Complete external-RPM radar client (lab sim + real backends).
class CheatClient {
 public:
  explicit CheatClient(sim::World& world,
                       std::string process_name = OBF("radar.exe"),
                       MemoryBackend backend_type = MemoryBackend::Sim);

  /// Move 1: open VM_READ on game (leaves blue-visible scar).
  bool attach_to_game(bool syscall_path = false);

  /// Move 2: parse lab entity table via RpmBackend + EntityPipeline.
  bool pull_entities();

  /// Move 3: project to external radar blips + optional overlay window scar.
  void render_radar(bool register_overlay = true);

  /// Full loop: attach → pull → render. Returns aggregate report.
  CheatClientReport run_full_loop(bool syscall_path = false,
                                  bool register_overlay = true);

  /// Advanced sim-only evasions. Their observable scars are intentional lab data.
  bool enable_multi_process_ipc_split();
  bool enable_windowless_swapchain();
  bool enable_scattered_reads();
  bool enable_handle_proxy(std::string proxy_name = OBF("nvidia-overlay.exe"));
  bool enable_entity_stream_crypto();
  bool enable_process_hollowing(std::string target_name = "svchost.exe");
  bool enable_apc_injection();
  bool enable_named_pipe_ipc();
  bool enable_registry_config();
  bool enable_binary_padding(std::size_t size);
  bool enable_steam_overlay_hijack();
  bool enable_window_hijack_overlay(
      const std::string& target_app = OBF("Discord.exe"));
  bool enable_pinned_swapchain(int wait_frames = 120);
  bool enable_vtable_present_fallback();
  bool enable_steam_critical_section();
  HandleGraphMonitorProbe probe_handle_graph_monitors();

  /// Advanced loop: split IPC + proxy-owned handle + windowless UI + scattered reads.
  CheatClientReport run_full_stealth_loop();

  /// Weak evasion helpers (do not remove handle graph scar).
  void disguise_name(std::string new_name);
  void hide_from_weak_process_enum();
  void throttle_mark(int hz);
  void close_and_reopen_brief();  // brief_reopen scar for multi-sample blue

  void detach();

  std::uint32_t pid() const { return pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  const std::vector<Blip>& blips() const { return blips_; }
  bool attached() const { return attached_; }
  RpmBackend& backend() { return backend_; }
  const RpmBackend& backend() const { return backend_; }
  RadarUi& ui() { return ui_; }
  const CheatClientReport& last_report() const { return last_; }
  MemoryBackend backend_type() const { return backend_type_; }

 private:
  int count_own_vm_read_() const;

  sim::World& world_;
  std::string name_;
  std::uint32_t pid_ = 0;
  std::uint32_t game_pid_ = 0;
  bool attached_ = false;
  MemoryBackend backend_type_{MemoryBackend::Sim};
  RpmBackend backend_;
  RadarUi ui_;
  std::vector<ac::EntitySnapshot> entities_;
  std::vector<Blip> blips_;
  CheatClientReport last_{};
  int throttle_hz_ = 0;
  ReadStrategy read_strategy_ = ReadStrategy::Sequential;

};

}  // namespace t0_red
