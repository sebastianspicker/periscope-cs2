#include "blue_example.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
bool has_module(const sim::Process& process, std::string_view module_name) {
  for (const auto& module : process.modules) {
    if (module.name == module_name) return true;
  }
  return false;
}
}  // namespace

bool self_obfuscate_process_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue,
        "Multi-reason: identity façade + game-access + handle/read co-occurrence.");

  std::vector<std::string> reasons;
  int disguised_readers = 0;
  int foreign_vm_read = 0;

  for (const auto& [pid, process] : w.processes) {
    if (process.name != "rtss.exe" && process.name != "Afterburner.exe" &&
        process.name != "discordoverlay.exe") {
      continue;
    }

    const bool identity_facade =
        process.window_class == "RTSSHooks" || process.peb_identity_matched ||
        has_module(process, "RTSSHooks64.dll");
    if (identity_facade) {
      reasons.emplace_back("disguised monitoring identity pid=" + std::to_string(pid) +
                           " name=" + process.name);
    }

    if (process.reader_active || process.timing_jittered) {
      reasons.emplace_back("disguised process reader/timing residual pid=" +
                           std::to_string(pid));
      ++disguised_readers;
    }

    for (const auto& h : w.handles_to(w.game_pid(), true)) {
      if (h.owner_pid != pid) continue;
      if (sim::has(h.access, sim::AccessMask::VmRead)) {
        ++foreign_vm_read;
        reasons.emplace_back("disguised process holds game VM_READ pid=" +
                             std::to_string(pid));
        break;
      }
    }
  }

  if (w.remote_read_ops > 0 || w.remote_read_bytes > 0) {
    reasons.emplace_back("remote read telemetry ops=" +
                         std::to_string(w.remote_read_ops));
  }

  // Deduplicate-ish: keep unique reasons as planted.
  const int signals = static_cast<int>(reasons.size());
  // Multi-reason gate: require at least two independent residual classes.
  const bool detected = signals >= 2 && (disguised_readers > 0 || foreign_vm_read > 0);

  std::printf("[blue:self_obfuscate_process] signals=%d disguised_readers=%d "
              "foreign_vm_read=%d detected=%d\n",
              signals, disguised_readers, foreign_vm_read, detected ? 1 : 0);
  for (const auto& r : reasons) {
    std::printf("[blue:self_obfuscate_process]   %s\n", r.c_str());
  }

  n.say(sim::Side::Blue,
        detected
            ? "Claimed monitoring tool identity co-occurs with game-access telemetry."
            : "Claimed process identities match baseline without access residual.");
  return detected;
}
