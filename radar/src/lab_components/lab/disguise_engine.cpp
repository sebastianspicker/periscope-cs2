#include "lab/disguise_engine.hpp"

#include <sstream>

namespace lab {

std::string_view DisguiseEngine::profile_name(ac::DisguiseProfile profile) {
  switch (profile) {
    case ac::DisguiseProfile::None: return "None";
    case ac::DisguiseProfile::SteamOverlay: return "SteamOverlay";
    case ac::DisguiseProfile::DiscordOverlay: return "DiscordOverlay";
    case ac::DisguiseProfile::RivaTuner: return "RivaTuner";
    case ac::DisguiseProfile::ObsStudio: return "ObsStudio";
    case ac::DisguiseProfile::NvidiaShadowplay: return "NvidiaShadowplay";
    case ac::DisguiseProfile::GenericMonitor: return "GenericMonitor";
  }
  return "Unknown";
}

DisguiseAttributes DisguiseEngine::attributes_for(ac::DisguiseProfile profile) {
  switch (profile) {
    case ac::DisguiseProfile::RivaTuner:
      return {"RTSS.exe", "RivaTunerWindow", "Global\\RTSS_HWMonitor",
              "Unwinder Software", 512.0, 48 * 1024, true, true};
    case ac::DisguiseProfile::DiscordOverlay:
      return {"Discord.exe", "Discord_Overlay", "Global\\DiscordIPC",
              "Discord Inc.", 256.0, 120 * 1024, true, false};
    case ac::DisguiseProfile::SteamOverlay:
      return {"gameoverlayui.exe", "SteamOverlayClass", "Global\\SteamOverlay",
              "Valve", 128.0, 64 * 1024, true, false};
    case ac::DisguiseProfile::ObsStudio:
      return {"obs64.exe", "OBSWindowClass", "Global\\OBSStudio",
              "OBS Project", 1024.0, 256 * 1024, true, false};
    case ac::DisguiseProfile::NvidiaShadowplay:
      return {"nvsphelper64.exe", "NVShadowPlayClass", "Global\\NVShadowPlay",
              "NVIDIA Corporation", 384.0, 96 * 1024, true, true};
    case ac::DisguiseProfile::GenericMonitor:
      return {"MSIAfterburner.exe", "MSIAfterburnerWndClass",
              "Global\\MSIAfterburner", "MSI Co., LTD", 192.0, 40 * 1024, true,
              true};
    case ac::DisguiseProfile::None:
    default:
      return {"unknown.exe", "DefaultWindow", "Global\\DefaultMutex", "", 64.0,
              8 * 1024, false, false};
  }
}

void DisguiseEngine::apply_to_world(sim::World& world, std::uint32_t cheat_pid,
                                    ac::DisguiseProfile profile) {
  auto* cheat = world.proc(cheat_pid);
  if (cheat == nullptr) return;

  const auto attrs = attributes_for(profile);
  cheat->name = attrs.process_name;
  cheat->window_class = attrs.window_class;
  cheat->working_set_kb = attrs.working_set_kb;
  cheat->looks_reputable = attrs.looks_reputable;
  cheat->signer = attrs.signer;
  cheat->peb_identity_matched = profile != ac::DisguiseProfile::None;

  world.hw_monitor_disguise_active = attrs.hw_monitor_family;
  world.hw_monitor_decoy_osd_active = attrs.hw_monitor_family;
  world.hw_monitor_rtss_hijack =
      profile == ac::DisguiseProfile::RivaTuner ||
      profile == ac::DisguiseProfile::GenericMonitor;

  // Mutex / named object surface residual for blue co-occurrence sensors.
  if (!attrs.mutex_name.empty()) {
    world.add_section(
        sim::SharedSection{attrs.mutex_name, cheat_pid, 0, false});
  }

  world.note(std::string("disguise_engine apply profile=") +
             std::string(profile_name(profile)) +
             " name=" + attrs.process_name +
             " class=" + attrs.window_class);
}

bool DisguiseEngine::verify_disguise(const sim::World& world,
                                     std::uint32_t cheat_pid,
                                     ac::DisguiseProfile expected) {
  return verify_report(world, cheat_pid, expected).verified;
}

DisguiseVerifyReport DisguiseEngine::verify_report(
    const sim::World& world, std::uint32_t cheat_pid,
    ac::DisguiseProfile expected) {
  DisguiseVerifyReport report;
  const auto* cheat = world.proc(cheat_pid);
  if (cheat == nullptr) {
    report.detail = "process not found";
    report.reasons.push_back("missing process");
    return report;
  }

  const auto attrs = attributes_for(expected);

  report.process_name_ok = cheat->name == attrs.process_name;
  report.window_class_ok = cheat->window_class == attrs.window_class;
  report.working_set_ok = cheat->working_set_kb == attrs.working_set_kb;
  report.reputation_ok = cheat->looks_reputable == attrs.looks_reputable &&
                         cheat->signer == attrs.signer;
  report.hw_flag_ok =
      world.hw_monitor_disguise_active == attrs.hw_monitor_family;

  if (report.process_name_ok) {
    ++report.signals_matched;
    report.reasons.push_back("process_name");
  }
  if (report.window_class_ok) {
    ++report.signals_matched;
    report.reasons.push_back("window_class");
  }
  if (report.working_set_ok) {
    ++report.signals_matched;
    report.reasons.push_back("working_set");
  }
  if (report.reputation_ok) {
    ++report.signals_matched;
    report.reasons.push_back("reputation/signer");
  }
  if (report.hw_flag_ok) {
    ++report.signals_matched;
    report.reasons.push_back("hw_monitor_flag");
  }

  report.verified = report.signals_matched >= 3;
  std::ostringstream oss;
  oss << "disguise signals=" << report.signals_matched << "/"
      << report.signals_total
      << " verified=" << (report.verified ? "1" : "0");
  report.detail = oss.str();
  return report;
}

}  // namespace lab
