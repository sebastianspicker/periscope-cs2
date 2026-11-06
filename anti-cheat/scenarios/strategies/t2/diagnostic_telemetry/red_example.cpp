#include "red_example.hpp"

#include <cstdio>

bool diagnostic_telemetry_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game for diagnostic telemetry evasion");
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (game == nullptr) return false;

  n.say(sim::Side::Red, "step 2: plant foreign manual-mapped module residual");
  game->modules.push_back({"telemetry_loader.dll", game->base + 0x700000, 0x3000,
                           false, true, "foreign", false, false, false});
  if (game->modules.empty() || game->modules.back().text_hash != "foreign") {
    return false;
  }

  n.say(sim::Side::Red, "step 3: plant foreign thread + mapped region scars");
  game->has_foreign_thread = true;
  game->manual_mapped_region = true;

  n.say(sim::Side::Red, "step 4: skew PE timestamp residual used by Message 159");
  w.pe_timestamp_client_dll ^= 0x11u;

  n.say(sim::Side::Red, "step 5: arm diagnostic system active surface");
  w.diagnostic_system_active = true;
  w.diagnostic_state.is_active = true;
  w.diagnostic_state.has_focus = true;

  n.say(sim::Side::Red, "step 6: plant thread capture residual for Message 159");
  auto& thread = w.diagnostic_state.thread_capture;
  thread.thread_id = game_pid;
  thread.module_name = "telemetry_loader.dll";
  thread.start_address = game->base + 0x700100;
  thread.from_suspicious_module = true;
  thread.from_rwx_memory = true;
  thread.memory_protection = 0x40;

  n.say(sim::Side::Red, "step 7: verify multi-scar plant retained");
  if (!(game->has_foreign_thread && game->manual_mapped_region &&
        w.diagnostic_system_active && thread.from_suspicious_module)) {
    return false;
  }

  std::printf(
      "[red:diagnostic_telemetry] achieved: foreign_mod+thread+timestamp+diag_active\n");
  w.note("diagnostic_telemetry: multi-scar Message 159 surface planted");
  return true;
}
