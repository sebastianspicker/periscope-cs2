#include "blue_example.hpp"

#include <sstream>
#include <string>

namespace examples::staged_loader {

BlueResult detect(sim::World& w) {
  BlueResult r;

  // Reason 1: offset-C2 style stage fetch flow.
  for (const auto& flow : w.net) {
    if (flow.looks_like_offset_c2) {
      r.reasons.emplace_back("net flow looks_like_offset_c2 stage fetch");
      break;
    }
  }

  // Reason 2: encrypted stage-two section residual.
  for (const auto& section : w.sections) {
    if (section.name.find("stage-two.encrypted") != std::string::npos) {
      r.reasons.emplace_back("encrypted stage-two shared section");
      break;
    }
  }

  // Reason 3: manual-mapped memory region (no disk image).
  for (const auto& [pid, process] : w.processes) {
    (void)pid;
    if (process.manual_mapped_region) {
      r.reasons.emplace_back("manual_mapped_region on loader process");
      break;
    }
  }

  // Reason 4: mapper / PEB-unlink style module residual.
  if (w.mapper_process_present) {
    r.reasons.emplace_back("mapper_process_present");
  }
  for (const auto& [pid, process] : w.processes) {
    (void)pid;
    for (const auto& module : process.modules) {
      if (!module.linked_in_peb || module.headers_erased ||
          module.text_hash.find("stage-") != std::string::npos) {
        r.reasons.emplace_back("stage payload module PEB/header residual");
        goto module_done;
      }
    }
  }
module_done:

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  r.mitigated = r.signals >= 3;
  if (r.mitigated) w.ranked_access_denied = true;

  std::ostringstream oss;
  oss << "staged_loader blue signals=" << r.signals
      << " detected=" << (r.detected ? 1 : 0)
      << " mitigated=" << (r.mitigated ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::staged_loader
