// evasion_advanced.cpp — composed T0 simulation lesson; never uses OS APIs.
//
// NOTE: The per-feature evasion methods (enable_temporal_jitter, etc.) set
// sim::World flags for educational lessons. For production evasion, use
// radar::FramePipeline which instantiates the actual ac_sim engines.

#include "t0_red/evasion_advanced.hpp"

namespace t0_red {

AdvancedEvasionReport AdvancedEvasionKit::max_stealth() {
  AdvancedEvasionReport report;
  if (auto* process = world_.proc(client_.pid())) {
    process->manual_mapped_region = true;
    process->modules.push_back({"t0-staged-payload", 0x50000000, 0x1000, false});
    report.staged_loader = true;
    report.steps.push_back("staged loader payload separated from the client image");
    world_.note("t0 AdvancedEvasionKit staged_loader payload=" +
                std::to_string(client_.pid()));
  }

  report.client_report = client_.run_full_stealth_loop();
  report.multi_process_split = report.client_report.multi_process_split;
  report.handle_proxy = report.client_report.handle_proxy;
  report.windowless_swapchain = report.client_report.windowless_swapchain;
  report.scattered_reads = report.client_report.scattered_reads;
  report.entity_stream_crypto = report.client_report.entity_stream_crypto;
  report.steps.push_back("handle holder, reader, and UI separated by entity IPC");
  report.steps.push_back("reputable proxy owns the VM_READ edge");
  report.steps.push_back("Present path marked without an external overlay window");
  report.steps.push_back("entity offsets read in shuffled order with timing jitter");
  report.steps.push_back("stream crypto teaches that a client-held key remains extractable");

  client_.hide_from_weak_process_enum();
  client_.throttle_mark(20);
  report.hidden_from_weak_enum = world_.proc(client_.pid()) &&
                                world_.proc(client_.pid())->hidden_from_weak_enum;
  report.throttled = true;
  report.steps.push_back("weak process enumeration suppressed and reads throttled to 20Hz");
  report.blue_sensor_probe = client_.probe_handle_graph_monitors();
  report.detail = "max_stealth staged=" + std::to_string(report.staged_loader) +
                  " split=" + std::to_string(report.multi_process_split) +
                  " proxy=" + std::to_string(report.handle_proxy) +
                  " blue=" + report.blue_sensor_probe.detail;
  world_.note("t0 AdvancedEvasionKit " + report.detail);
  return report;
}

AdvancedEvasionReport AdvancedEvasionKit::deep_stealth() {
  AdvancedEvasionReport report;
  if (!client_.enable_process_hollowing() || !client_.enable_apc_injection() ||
      !client_.enable_multi_process_ipc_split() || !client_.enable_handle_proxy() ||
      !client_.enable_windowless_swapchain() || !client_.enable_scattered_reads() ||
      !client_.enable_named_pipe_ipc() || !client_.enable_entity_stream_crypto() ||
      !client_.enable_registry_config() || !client_.enable_binary_padding(8192)) {
    report.client_report = client_.last_report();
    report.detail = "deep_stealth_setup_failed";
    world_.note("t0 AdvancedEvasionKit " + report.detail);
    return report;
  }

  client_.render_radar(false);
  client_.hide_from_weak_process_enum();
  client_.throttle_mark(20);

  report.client_report = client_.last_report();
  report.process_hollowing = world_.process_hollowing_active;
  report.apc_injection = world_.apc_injection_active;
  report.multi_process_split = world_.multi_process_split_active;
  report.handle_proxy = world_.handle_proxy_active;
  report.windowless_swapchain = world_.windowless_swapchain_hijack;
  report.scattered_reads = world_.scattered_read_pattern;
  report.named_pipe_ipc = world_.named_pipe_ipc_active;
  report.entity_stream_crypto = world_.entity_stream_encrypted;
  report.registry_config = world_.registry_persisted_config;
  report.binary_padding = world_.binary_padding_applied;
  report.hidden_from_weak_enum = world_.proc(client_.pid()) &&
                                world_.proc(client_.pid())->hidden_from_weak_enum;
  report.throttled = true;
  report.steps = {
      "process hollowed into a reputable image",
      "APC injection marked a foreign game thread",
      "handle holder, reader, and UI split under the hollowed parent",
      "reputable proxy owns the VM_READ edge",
      "Present path marked without an external overlay window",
      "entity offsets read in shuffled order with timing jitter",
      "entity relay moved to a named pipe scar",
      "stream crypto retains a client-held key",
      "configuration persisted in the simulated registry",
      "binary padded to alter its simulated hash",
      "weak process enumeration suppressed and reads throttled to 20Hz",
  };
  report.blue_sensor_probe = client_.probe_handle_graph_monitors();
  report.detail = "deep_stealth hollow=" +
                  std::to_string(report.process_hollowing) +
                  " apc=" + std::to_string(report.apc_injection) +
                  " split=" + std::to_string(report.multi_process_split) +
                  " pipe=" + std::to_string(report.named_pipe_ipc) +
                  " blue=" + report.blue_sensor_probe.detail;
  world_.note("t0 AdvancedEvasionKit " + report.detail);
  return report;
}

AdvancedEvasionReport AdvancedEvasionKit::deep_stealth_v2() {
  AdvancedEvasionReport report;
  const bool steam_hooked = client_.enable_steam_overlay_hijack();
  if ((!steam_hooked && !client_.enable_vtable_present_fallback()) ||
      (steam_hooked && !client_.enable_steam_critical_section()) ||
      !client_.enable_window_hijack_overlay() ||
      !client_.enable_pinned_swapchain()) {
    report.client_report = client_.last_report();
    report.detail = "deep_stealth_v2_overlay_setup_failed";
    world_.note("t0 AdvancedEvasionKit " + report.detail);
    return report;
  }

  report = deep_stealth();
  report.client_report = client_.last_report();
  report.steam_overlay_hijack_active = world_.steam_present_hooked;
  report.window_hijack_active = world_.window_hijacked;
  report.client_report.steam_overlay_hijack = world_.steam_present_hooked;
  report.client_report.window_hijack_overlay = world_.window_hijacked;
  report.client_report.steam_hook_present = world_.steam_present_hooked;
  report.client_report.steam_hook_resize = world_.steam_resize_hooked;
  report.client_report.swapchain_pinned = world_.swapchain_pinned;
  report.client_report.pin_wait_frames = world_.pin_wait_frames;
  report.client_report.vtable_fallback = world_.vtable_present_hook_fallback;
  report.client_report.steam_critical_section =
      world_.steam_critical_section_active;
  report.steps.insert(report.steps.begin(),
                      "Steam overlay Present and Resize simulation enabled");
  report.steps.insert(report.steps.begin() + 1,
                      "foreign overlay HWND presentation registered for PID comparison");
  report.steps.insert(report.steps.begin() + 2,
                      "swapchain rendering pinned after 120 simulated frames");
  if (world_.vtable_present_hook_fallback) {
    report.steps.insert(report.steps.begin() + 1,
                        "Steam unavailable: game Present fallback simulated");
  } else {
    report.steps.insert(report.steps.begin() + 1,
                        "Steam CriticalSection simulation marked active");
  }
  report.detail = "deep_stealth_v2 steam=" +
                  std::to_string(report.steam_overlay_hijack_active) +
                  " window_hijack=" +
                  std::to_string(report.window_hijack_active) +
                  " pinned=" + std::to_string(world_.swapchain_pinned) +
                  " " + report.blue_sensor_probe.detail;
  world_.note("t0 AdvancedEvasionKit " + report.detail);
  return report;
}

// ── Enhanced evasion methods ─────────────────────────────────

bool AdvancedEvasionKit::enable_temporal_jitter(int min_ms, int max_ms) {
  if (min_ms < 0 || max_ms < min_ms) return false;
  world_.read_timing_jitter = true;
  world_.batch_read_jittered = true;
  world_.temporal_phase_active = true;
  world_.note("t0 AdvancedEvasionKit temporal_jitter min=" +
              std::to_string(min_ms) + " max=" + std::to_string(max_ms));
  return true;
}

bool AdvancedEvasionKit::enable_batch_shuffle() {
  // Mark the world to shuffle read order for evasion
  world_.scattered_read_pattern = true;
  world_.note("t0 AdvancedEvasionKit batch_shuffle enabled");
  return true;
}

bool AdvancedEvasionKit::enable_memory_hide() {
  // Unlink from PEB: hide mapped regions from enumeration
  if (auto* process = world_.proc(client_.pid())) {
    process->manual_mapped_region = true;
    for (auto& mod : process->modules) {
      mod.linked_in_peb = false;
      mod.headers_erased = true;
    }
    world_.note("t0 AdvancedEvasionKit memory_hide pid=" +
                std::to_string(client_.pid()));
    return true;
  }
  return false;
}

bool AdvancedEvasionKit::enable_decoy_render(int count) {
  // Create fake overlay windows to confuse blue overlay detection
  world_.decoy_render_active = true;
  world_.ml_confusion_active = true;
  for (int i = 0; i < count; ++i) {
    std::string decoy_title = "DecoyWindow_" + std::to_string(i);
    auto decoy_pid = world_.spawn(decoy_title, false, false, client_.pid());
    world_.add_overlay(sim::OverlayWindow{decoy_pid, decoy_title,
                                          true, true, false});
  }
  world_.note("t0 AdvancedEvasionKit decoy_render count=" +
              std::to_string(count));
  return true;
}

}  // namespace t0_red
