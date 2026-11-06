#include "blue_example.hpp"

#include "cs2/diagnostic_sensors.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kClientDllTimestamp = 0x66800000u;

bool is_foreign_manual_mapped(const sim::Module& m) {
  return !m.linked_in_peb || m.headers_erased;
}

}  // namespace

bool pe_timestamp_clone_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue, "Multi-reason: PE timestamps + foreign module identity + map residual.");

  std::vector<std::string> reasons;

  n.say(sim::Side::Blue, "sensor 1: PE timestamp verification for known modules");
  const auto report = cs2::sensors::PETimestampSensor(w).verify();
  if (report.any_mismatch) {
    reasons.emplace_back("known_module_timestamp_mismatch count=" +
                         std::to_string(report.mismatched_count));
  }

  n.say(sim::Side::Blue, "sensor 2: module identity cross-check for cloned timestamps");
  int foreign_with_clone = 0;
  for (const auto& [pid, process] : w.processes) {
    for (const auto& module : process.modules) {
      if (!is_foreign_manual_mapped(module)) continue;
      if (module.name == "cheat_core.dll" || module.text_hash != "clean") {
        ++foreign_with_clone;
        reasons.emplace_back("foreign_manual_mapped module=" + module.name +
                             " pid=" + std::to_string(pid));
      }
    }
  }

  n.say(sim::Side::Blue, "sensor 3: diagnostic snapshot cross-check");
  bool snapshot_clone = false;
  for (const auto& info : w.diagnostic_state.loaded_modules) {
    if (info.name == "cheat_core.dll" && info.pe_timestamp == kClientDllTimestamp) {
      snapshot_clone = true;
      reasons.emplace_back("snapshot_cloned_client_dll_timestamp on cheat_core.dll");
    }
  }
  if (w.diag_pe_timestamps_sent) {
    reasons.emplace_back("diag_pe_timestamps_sent");
  }

  if (const auto* game = w.proc(w.game_pid())) {
    if (game->manual_mapped_region) reasons.emplace_back("manual_mapped_region");
    if (game->has_foreign_thread) reasons.emplace_back("foreign_thread_co_scar");
  }

  const int signals = static_cast<int>(reasons.size());
  const bool detected =
      signals >= 2 && (foreign_with_clone > 0 || snapshot_clone || report.any_mismatch);

  std::printf("[blue:pe_timestamp_clone] foreign=%d snapshot_clone=%d signals=%d "
              "detected=%d\n",
              foreign_with_clone, snapshot_clone ? 1 : 0, signals, detected ? 1 : 0);
  for (const auto& r : reasons) {
    std::printf("[blue:pe_timestamp_clone]   %s\n", r.c_str());
  }

  if (detected) {
    n.say(sim::Side::Blue,
          "DETECTED: multi-reason — foreign module borrows legitimate PE timestamp.");
  } else {
    n.say(sim::Side::Blue, "no multi-reason cloned-timestamp residual observed");
  }
  return detected;
}
