#include "red_example.hpp"

#include "cs2/diagnostic_sensors.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

constexpr std::uint32_t kClientDllTimestamp = 0x66800000u;
constexpr std::uint32_t kForeignTimestamp = 0x5F000000u;

}  // namespace

bool pe_timestamp_clone_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game for PE timestamp clone");
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (game == nullptr || !game->is_game) return false;

  n.say(sim::Side::Red, "step 2: plant foreign manual-mapped cheat module");
  sim::Module cheat;
  cheat.name = "cheat_core.dll";
  cheat.base = game->base + 0x9A0000;
  cheat.size = 0x5000;
  cheat.linked_in_peb = false;
  cheat.headers_erased = false;
  cheat.text_hash = "clean";
  game->modules.push_back(cheat);
  if (game->modules.empty() || game->modules.back().name != "cheat_core.dll") {
    return false;
  }

  n.say(sim::Side::Red, "step 3: record foreign (non-cloned) PE timestamp baseline");
  cs2::DiagnosticModuleInfo baseline;
  baseline.process_id = game_pid;
  baseline.name = "cheat_core.dll";
  baseline.base = cheat.base;
  baseline.size = cheat.size;
  baseline.pe_timestamp = kForeignTimestamp;
  baseline.expected = false;
  baseline.suspicious = true;
  w.diagnostic_state.loaded_modules.push_back(baseline);

  n.say(sim::Side::Red, "step 4: clone legitimate client.dll timestamp onto cheat module");
  bool cloned = false;
  for (auto& info : w.diagnostic_state.loaded_modules) {
    if (info.name == "cheat_core.dll") {
      info.pe_timestamp = kClientDllTimestamp;
      info.expected = true;
      info.suspicious = false;
      cloned = true;
    }
  }
  if (!cloned) return false;

  n.say(sim::Side::Red, "step 5: plant co-scars — unlinked map + manual region residual");
  game->manual_mapped_region = true;
  game->has_foreign_thread = true;
  w.module_loaded = true;
  w.module_is_signed = false;

  n.say(sim::Side::Red, "step 6: run the simulated PETimestampSensor against the clone");
  const auto report = cs2::sensors::PETimestampSensor(w).verify();
  if (report.any_mismatch) {
    n.say(sim::Side::Red, "timestamp sensor still sees a mismatch; clone failed");
    return false;
  }
  w.diag_pe_timestamps_sent = true;
  w.note("pe_timestamp_clone: cheat_core.dll now presents client.dll timestamp 0x66800000");
  n.say(sim::Side::Red, "achieved: cloned PE timestamp passes timestamp verification");
  std::printf("[red:pe_timestamp_clone] achieved: clone_ok unlinked=1 mapped=1\n");
  return true;
}
