// cheat_client_evasion.cpp — T0 red evasion / stealth enable paths.

#include "t0_red/cheat_client.hpp"

#include <cstdio>
#include <chrono>
#include <cstring>
#include <sstream>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace t0_red {

namespace {

std::uint64_t section_name_seed() noexcept {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  return __rdtsc();
#else
  return static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

}  // namespace

bool CheatClient::enable_multi_process_ipc_split() {
  if (world_.multi_process_split_active) {
    return world_.split_holder_pid != 0 && world_.split_reader_pid != 0 &&
           world_.split_ui_pid != 0;
  }
  game_pid_ = world_.game_pid();
  auto* game = world_.proc(game_pid_);
  if (!game || !game->is_game) {
    last_.detail = "no_game";
    return false;
  }
  if (game->memory.size() < 4 ||
      (game->memory[0] == 0 && game->memory[1] == 0 &&
       game->memory[2] == 0 && game->memory[3] == 0)) {
    world_.plant_lab_entities(game_pid_);
  }

  const auto holder = world_.spawn("svchost.exe", false, false, pid_);
  const auto reader = world_.spawn("radar-reader.exe", false, false, pid_);
  const auto ui = world_.spawn("radar-ui.exe", false, false, pid_);
  if (!world_.proc(holder) || !world_.proc(reader) || !world_.proc(ui) ||
      backend_.attach_world(world_, holder, game_pid_) != ac::Status::Ok) {
    last_.detail = "ipc_split_setup_failed";
    return false;
  }
  world_.proc(holder)->looks_reputable = true;
  world_.proc(reader)->reader_active = true;
  uint64_t rng = section_name_seed() ^ 0xA5A5A5A5A5A5A5A5ULL;
  rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
  char sectionName[64];
  std::snprintf(sectionName, 64, "Local\\%016llx", static_cast<unsigned long long>(rng));
  world_.add_section({sectionName, holder, reader, true});
  world_.add_section({"reader-to-ui-entities", reader, ui, true});
  world_.multi_process_split_active = true;
  world_.split_holder_pid = holder;
  world_.split_reader_pid = reader;
  world_.split_ui_pid = ui;
  attached_ = true;
  last_.attached = true;
  last_.multi_process_split = true;
  last_.handle_holder_pid = holder;
  last_.reader_pid = reader;
  last_.ui_pid = ui;
  world_.note("t0 CheatClient multi_process_ipc_split holder=" +
              std::to_string(holder) + " reader=" + std::to_string(reader) +
              " ui=" + std::to_string(ui));
  return true;
}

bool CheatClient::enable_handle_proxy(std::string proxy_name) {
  game_pid_ = world_.game_pid();
  if (!world_.proc(game_pid_)) {
    last_.detail = "no_game";
    return false;
  }
  std::uint32_t proxy = world_.split_holder_pid;
  const std::uint32_t consumer = world_.split_reader_pid ? world_.split_reader_pid : pid_;
  if (!proxy) {
    proxy = world_.spawn(std::move(proxy_name));
    if (auto* process = world_.proc(proxy)) {
      process->looks_reputable = true;
    }
  }
  if (backend_.reader_pid() != proxy || !backend_.is_attached()) {
    if (backend_.attach_world(world_, proxy, game_pid_) != ac::Status::Ok) {
      last_.detail = "proxy_attach_failed";
      return false;
    }
  }
  for (auto& handle : world_.handles) {
    if (handle.owner_pid == proxy && handle.target_pid == game_pid_ &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      handle.via_proxy = true;
    }
  }
  uint64_t rng = section_name_seed() ^ 0xA5A5A5A5A5A5A5A5ULL;
  rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
  char sectionName[64]{};
  std::snprintf(sectionName, sizeof(sectionName), "Local\\%016llx",
                static_cast<unsigned long long>(rng));
  world_.add_section({sectionName, proxy, consumer, true});
  world_.handle_proxy_active = true;
  world_.handle_proxy_owner_pid = proxy;
  world_.handle_proxy_consumer_pid = consumer;
  attached_ = true;
  last_.attached = true;
  last_.handle_proxy = true;
  last_.handle_holder_pid = proxy;
  world_.note("t0 CheatClient handle_proxy owner=" + std::to_string(proxy) +
              " consumer=" + std::to_string(consumer));
  return true;
}

bool CheatClient::enable_windowless_swapchain() {
  game_pid_ = world_.game_pid();
  auto* game = world_.proc(game_pid_);
  if (!game || !game->is_game || game->modules.empty()) {
    last_.detail = "no_game_present_path";
    return false;
  }
  game->modules.front().present_hooked = true;
  world_.windowless_swapchain_hijack = true;
  world_.swapchain_hijacked_no_window = true;
  const auto owner = world_.split_ui_pid ? world_.split_ui_pid : pid_;
  // An internal marker models the intercepted Present path; it is not a HWND.
  world_.add_overlay({owner, "swapchain-internal-marker", false, false, true});
  last_.windowless_swapchain = true;
  world_.note("t0 CheatClient windowless_swapchain Present hook no_window");
  return true;
}

bool CheatClient::enable_scattered_reads() {
  if (!attached_) {
    last_.detail = "not_attached";
    return false;
  }
  read_strategy_ = ReadStrategy::Scattered;
  world_.read_timing_jitter = true;
  const bool read_ok = pull_entities();
  last_.scattered_reads = read_ok && world_.scattered_read_pattern;
  if (read_ok) {
    world_.note("t0 CheatClient scattered_entity_reads count=" +
                std::to_string(world_.scattered_read_count) + " timing_jitter=1");
  }
  return read_ok;
}

bool CheatClient::enable_entity_stream_crypto() {
  world_.entity_stream_encrypted = true;
  world_.client_has_stream_key = true;
  world_.stream_key_exfiltrated = true;
  last_.entity_stream_crypto = true;
  world_.note("t0 CheatClient entity_stream_crypto client_key_extracted");
  return true;
}

bool CheatClient::enable_process_hollowing(std::string target_name) {
  if (target_name.empty()) {
    last_.detail = "hollow_target_empty";
    return false;
  }
  if (world_.process_hollowing_active) {
    const auto* hollowed = world_.proc(world_.hollowed_pid);
    if (!hollowed) {
      last_.detail = "hollowed_process_missing";
      return false;
    }
    pid_ = world_.hollowed_pid;
    name_ = hollowed->name;
    ui_.set_title(name_);
    if (world_.window_hijacked) {
      world_.presenter_pid = pid_;
      world_.cross_process_hijack = pid_ != world_.hwnd_owner_pid;
    }
    for (const auto child_pid : {world_.split_reader_pid, world_.split_ui_pid}) {
      if (auto* child = world_.proc(child_pid)) {
        child->parent_pid = pid_;
      }
    }
    last_.process_hollowing = true;
    last_.hollowed_pid = pid_;
    world_.note("t0 CheatClient process_hollowing existing pid=" +
                std::to_string(pid_));
    return true;
  }

  const auto hollowed_pid = world_.spawn(target_name, false, false, pid_);
  auto* hollowed = world_.proc(hollowed_pid);
  if (!hollowed) {
    last_.detail = "hollow_spawn_failed";
    return false;
  }
  backend_.detach();
  attached_ = false;
  hollowed->hollowed = true;
  hollowed->original_image = target_name;
  hollowed->looks_reputable = true;
  world_.process_hollowing_active = true;
  world_.hollowed_pid = hollowed_pid;
  world_.hollowed_original_image = target_name;
  pid_ = hollowed_pid;
  name_ = target_name;
  ui_.set_title(name_);
  if (world_.window_hijacked) {
    world_.presenter_pid = pid_;
    world_.cross_process_hijack = pid_ != world_.hwnd_owner_pid;
  }
  for (const auto child_pid : {world_.split_reader_pid, world_.split_ui_pid}) {
    if (auto* child = world_.proc(child_pid)) {
      child->parent_pid = pid_;
    }
  }
  last_.process_hollowing = true;
  last_.hollowed_pid = hollowed_pid;
  world_.note("t0 CheatClient process_hollowing pid=" +
              std::to_string(hollowed_pid) + " image=" + target_name);
  return true;
}

bool CheatClient::enable_apc_injection() {
  game_pid_ = world_.game_pid();
  auto* game = world_.proc(game_pid_);
  if (!game || !game->is_game) {
    last_.detail = "no_game";
    return false;
  }
  game->has_foreign_thread = true;
  game->thread_hijacked = true;
  world_.apc_injection_active = true;
  world_.apc_injection_count = 1;
  last_.apc_injection = true;
  world_.note("t0 CheatClient apc_injection game=" + std::to_string(game_pid_));
  return true;
}

bool CheatClient::enable_named_pipe_ipc() {
  const auto producer = world_.split_holder_pid ? world_.split_holder_pid : pid_;
  const auto consumer = world_.split_reader_pid ? world_.split_reader_pid : pid_;
  if (!world_.proc(producer) || !world_.proc(consumer)) {
    last_.detail = "pipe_endpoint_missing";
    return false;
  }
  if (!world_.named_pipe_ipc_active) {
    world_.add_section({"pipe:t0-entity-relay", producer, consumer, true});
  }
  world_.named_pipe_ipc_active = true;
  world_.named_pipe_message_count = 1;
  last_.named_pipe_ipc = true;
  world_.note("t0 CheatClient named_pipe_ipc producer=" +
              std::to_string(producer) + " consumer=" + std::to_string(consumer));
  return true;
}

bool CheatClient::enable_registry_config() {
  world_.registry_persisted_config = true;
  world_.registry_key_path =
      "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\T0Radar";
  last_.registry_config = true;
  world_.note("t0 CheatClient registry_config key=" + world_.registry_key_path);
  return true;
}

bool CheatClient::enable_binary_padding(std::size_t size) {
  world_.binary_padding_applied = true;
  world_.padded_binary_size = size;
  last_.binary_padding = true;
  world_.note("t0 CheatClient binary_padding size=" + std::to_string(size));
  return true;
}

bool CheatClient::enable_steam_overlay_hijack() {
  game_pid_ = world_.game_pid();
  auto* game = world_.proc(game_pid_);
  if (!game || !game->is_game || game->modules.empty()) {
    last_.detail = "no_game_present_path";
    return false;
  }

  // The lab loads the overlay and treats that transition as Steam populating
  // its simulated trampoline slots; no module memory is accessed or changed.
  world_.gameoverlay_loaded = true;
  world_.steam_original_trampolines_saved = true;
  world_.steam_trampoline_hooks_written = true;
  world_.gameoverlay_base = 0x70000000ull;
  world_.original_steam_present_ptr = world_.gameoverlay_base + 0x1000;
  world_.original_steam_resize_buffers_ptr = world_.gameoverlay_base + 0x1100;
  world_.hook_present_fn = 0x50000000ull + pid_;
  world_.hook_resize_buffers_fn = 0x50001000ull + pid_;
  game->modules.front().present_hooked = true;
  world_.steam_present_hooked = true;
  world_.steam_resize_hooked = true;
  world_.steam_resize_buffers_hooked = true;
  last_.steam_overlay_hijack = true;
  last_.steam_hook_present = true;
  last_.steam_hook_resize = true;
  world_.note("t0 CheatClient steam_overlay_hijack overlay_loaded=1 "
              "present_offset=0x162200 resize_offset=0x162208 simulated=1");
  return true;
}

bool CheatClient::enable_window_hijack_overlay(const std::string& target_app) {
  if (target_app.empty()) {
    last_.detail = "overlay_target_empty";
    return false;
  }

  std::uint32_t target_pid = 0;
  for (const auto& [pid, process] : world_.processes) {
    if (process.name == target_app) {
      target_pid = pid;
      break;
    }
  }
  if (!target_pid) {
    target_pid = world_.spawn(target_app);
  }
  if (!world_.proc(target_pid)) {
    last_.detail = "overlay_target_missing";
    return false;
  }

  world_.window_hijacked = true;
  world_.hwnd_owner_pid = target_pid;
  world_.presenter_pid = pid_;
  world_.cross_process_hijack = world_.presenter_pid != world_.hwnd_owner_pid;
  world_.swap_chain_count = 1;
  world_.swap_chain_output_window = 0x10000000ull + target_pid;
  world_.output_window_owner_pid = target_pid;
  world_.cross_process_swapchain = world_.cross_process_hijack;
  ui_.present_hijacked_overlay(world_, pid_, target_pid);
  last_.window_hijack_overlay = true;
  world_.note("t0 CheatClient window_hijack_overlay target=" + target_app +
              " presenter=" + std::to_string(pid_) + " hwnd_owner=" +
              std::to_string(target_pid));
  return true;
}

bool CheatClient::enable_pinned_swapchain(int wait_frames) {
  if (wait_frames < 0) {
    last_.detail = "pin_wait_frames_negative";
    return false;
  }
  world_.swapchain_pinned = true;
  world_.pin_wait_frames = wait_frames;
  last_.swapchain_pinned = true;
  last_.pin_wait_frames = wait_frames;
  world_.note("t0 CheatClient pinned_swapchain wait_frames=" +
              std::to_string(wait_frames));
  return true;
}

bool CheatClient::enable_vtable_present_fallback() {
  game_pid_ = world_.game_pid();
  auto* game = world_.proc(game_pid_);
  if (!game || !game->is_game || game->modules.empty()) {
    last_.detail = "no_game_present_path";
    return false;
  }
  game->modules.front().present_hooked = true;
  world_.vtable_present_hook_fallback = true;
  last_.vtable_fallback = true;
  world_.note("t0 CheatClient vtable_present_fallback game_present_hooked=1");
  return true;
}

bool CheatClient::enable_steam_critical_section() {
  world_.steam_critical_section_active = true;
  last_.steam_critical_section = true;
  world_.note("t0 CheatClient steam_critical_section active=1");
  return true;
}

HandleGraphMonitorProbe CheatClient::probe_handle_graph_monitors() {
  HandleGraphMonitorProbe probe;
  const auto ac = world_.ac_pid();
  probe.ac_process_present = ac != 0;
  const auto handles = world_.handles_to(game_pid_, false);
  probe.observable_game_handles = static_cast<int>(handles.size());
  for (const auto& handle : handles) {
    probe.ac_has_game_handle = probe.ac_has_game_handle || handle.owner_pid == ac;
  }
  probe.blue_handle_sampling_likely = probe.ac_process_present &&
                                      probe.observable_game_handles > 0;
  std::ostringstream oss;
  oss << "ac_present=" << probe.ac_process_present
      << " ac_game_handle=" << probe.ac_has_game_handle
      << " observable_game_handles=" << probe.observable_game_handles;
  probe.detail = oss.str();
  world_.note("t0 CheatClient probe_handle_graph_monitors " + probe.detail);
  return probe;
}

CheatClientReport CheatClient::run_full_stealth_loop() {
  last_ = {};
  if (!enable_multi_process_ipc_split() || !enable_handle_proxy() ||
      !enable_windowless_swapchain() || !enable_scattered_reads() ||
      !enable_entity_stream_crypto()) {
    return last_;
  }
  render_radar(false);
  last_.foreign_vm_read_handles = count_own_vm_read_();
  last_.bytes_read = backend_.bytes_read_total();
  last_.read_ops = backend_.read_ops();
  last_.multi_process_split = world_.multi_process_split_active;
  last_.handle_proxy = world_.handle_proxy_active;
  last_.windowless_swapchain = world_.windowless_swapchain_hijack;
  last_.scattered_reads = world_.scattered_read_pattern;
  last_.entity_stream_crypto = world_.entity_stream_encrypted;
  last_.handle_holder_pid = world_.split_holder_pid;
  last_.reader_pid = world_.split_reader_pid;
  last_.ui_pid = world_.split_ui_pid;
  const auto probe = probe_handle_graph_monitors();
  last_.detail = "full_stealth_loop split=1 proxy=1 windowless=1 scattered=1 " +
                 probe.detail;
  world_.note("t0 CheatClient " + last_.detail);
  return last_;
}

// CheatClient::disguise_name: Weak rename of red process name (does not hide handles).
void CheatClient::disguise_name(std::string new_name) {
  if (auto* p = world_.proc(pid_)) {
    p->name = std::move(new_name);
    name_ = p->name;
    ui_.set_title(name_);
  }
}

// CheatClient::hide_from_weak_process_enum: Weak name-hide; does not clear handle graph.
void CheatClient::hide_from_weak_process_enum() {
  if (auto* p = world_.proc(pid_)) {
    p->hidden_from_weak_enum = true;
  }
}

// CheatClient::throttle_mark: Mark reads as throttled (rate residual only).
void CheatClient::throttle_mark(int hz) {
  throttle_hz_ = hz;
  world_.note("t0 CheatClient read_throttle hz=" + std::to_string(hz));
}

// CheatClient::close_and_reopen_brief: Briefly close/reopen VmRead handle (weak hide lesson).
void CheatClient::close_and_reopen_brief() {
  if (!attached_ || !game_pid_) {
    return;
  }
  // Hide during AC enum then reopen — multi-sample race scar.
  for (auto& h : world_.handles) {
    if (h.owner_pid == backend_.reader_pid() && h.target_pid == game_pid_) {
      h.hidden_during_enum = true;
      h.brief_reopen = true;
    }
  }
  world_.note("t0 CheatClient close_and_reopen_brief hide+brief_reopen");
}

}  // namespace t0_red
