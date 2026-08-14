#include "blue_example.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace examples::convars_temp_restore {

bool detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue, "Multi-reason: live ConVar CRC + continuous window residual + co-scars.");

  std::vector<std::string> reasons;

  n.say(sim::Side::Blue, "sensor 1: live ConVar integrity (Message 157 CRC)");
  bool live_tampered = false;
  bool convar_seen = false;
  for (const auto& c : w.diagnostic_state.convars) {
    if (c.name == "sv_cheats") convar_seen = true;
    if (c.tampered || (c.name == "sv_cheats" && c.value != "0")) {
      live_tampered = true;
    }
  }
  if (live_tampered) reasons.emplace_back("live_convar_tampered");
  if (convar_seen) reasons.emplace_back("sv_cheats_surface_present");

  n.say(sim::Side::Blue, "sensor 2: continuous/trigger residual from technique window");
  bool window_residual = false;
  bool continuous_crc = false;
  for (const auto& line : w.log) {
    if (line.find("convars_temp_restore") != std::string::npos ||
        line.find("sv_cheats=1") != std::string::npos ||
        line.find("technique window") != std::string::npos) {
      window_residual = true;
    }
    if (line.find("continuous_crc_sample_dirty") != std::string::npos) {
      continuous_crc = true;
    }
  }
  if (window_residual) reasons.emplace_back("technique_window_residual");
  if (continuous_crc) reasons.emplace_back("continuous_crc_sample_dirty");
  if (w.diag_convar_checked) reasons.emplace_back("diag_convar_checked residual");

  n.say(sim::Side::Blue, "sensor 3: foreign handle / remote-read co-occurrence");
  int foreign = 0;
  for (const auto& h : w.handles_to(w.game_pid(), true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) ++foreign;
  }
  if (foreign > 0) reasons.emplace_back("foreign_VM_READ=" + std::to_string(foreign));
  if (w.remote_read_ops > 0) {
    reasons.emplace_back("remote_read_ops=" + std::to_string(w.remote_read_ops));
  }

  const int signals = static_cast<int>(reasons.size());
  // Prefer continuous residual after restore — multi-reason lesson.
  const bool detected =
      signals >= 2 && (live_tampered || window_residual || continuous_crc);

  std::printf(
      "[blue:convars_temp_restore] live=%d window=%d crc=%d signals=%d detected=%d\n",
      live_tampered ? 1 : 0, window_residual ? 1 : 0, continuous_crc ? 1 : 0, signals,
      detected ? 1 : 0);
  for (const auto& r : reasons) {
    std::printf("[blue:convars_temp_restore]   %s\n", r.c_str());
  }

  if (detected) {
    n.say(sim::Side::Blue,
          "DETECTED: multi-reason residual caught modify window after restore.");
  } else {
    n.say(sim::Side::Blue, "ConVar CRC matches — insufficient multi-reason residual.");
  }
  return detected;
}

}  // namespace examples::convars_temp_restore
